#include "fishCenS.h"

#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <pigpio.h>
#include <string>
#include <filesystem>
#include <sstream>
#include <regex>

// Add simple GUI elements
#define CVUI_DISABLE_COMPILATION_NOTICES
#define CVUI_IMPLEMENTATION
#include "cvui.h"

namespace fs = std::filesystem;

FishCenS::FishCenS()
{
}


FishCenS::~FishCenS()
{
	_log("Closing stream...");
	destroyAllWindows();
	_cam.stopVideo();
	gpioWrite(LED_PIN, 0);
}

/**
	Overall run of thread
	Generally:
	init() is called first in main.cpp,
	then this method is run continuously.
	update() takes care of any movement, tracking, sensors, etc
	draw() collects all the image data and presents it to the screen, including things like labelling, concatenating mats, etc.
	
	Later, this may be threaded, particularly draw(). 
 **/

int FishCenS::run()
{
	while (_returnKey != 27)
	{
		update();
		draw();
	}
	
	//Temporary - save log file
	_saveLogFile();

	return 1;
}

int FishCenS::init(fcMode mode)
{	
	//Mode of fishCenS object
	_mode = mode;
	
	//Return key from waitKey
	_returnKey = '\0';
	
	//Load filename and filepath
	_logFileName = _getTime();
	
	//Clear timers and load start timer
	_timers.clear();
	_timers["runTime"] = _millis();
	_timers["drawTime"] = _millis();
	_timers["depthTimer"] = _millis();
	_timers["tempTimer"] = _millis();
	
	//Tracking and fish counting
	_fishCount = 0;
	_fishTrackerObj.init(VIDEO_WIDTH, VIDEO_HEIGHT);
	
	//Video stuff that needs to be initialized
	_videoRecordState = vrMode::VIDEO_OFF;
	
	//Make sure no threads
	_threadVector.clear();
	
	//initiate camera	
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION || _mode == fcMode::VIDEO_RECORDER)
	{
		_cam.options->video_width = VIDEO_WIDTH;
		_cam.options->video_height = VIDEO_HEIGHT;
		_cam.options->verbose = 1;
		_cam.options->list_cameras = true;
		_cam.options->framerate = 100;
		_cam.startVideo();
		
		//Allow camera time to figure itself out
		_log("Attempting to start camera. Sleeping for " + to_string(SLEEP_TIMER) + "ms", true);
		double camTimer = _millis();
		while((_millis() - camTimer) < SLEEP_TIMER)
		{
		}

		//Load video frame, if timeout, restart while loop
		if (!_cam.getVideoFrame(_frame, 1000))
		{
			_log("Timeout error while initializing", true);
		}
			
		//If there's nothing in the video frame, also exit
		if (_frame.empty())
		{
			_log("Camera doesn't work, frame empty", true);
			return -1;
		}
		
		//Set class video width and height for tracker
		_videoWidth = _frame.size().width;
		_videoHeight = _frame.size().height;
	
		_log("Camera found. Size of camera is: ", true);
		_log("\t>> Width: " + to_string(_videoWidth) + "px", true);
		_log("\t>> Width: " + to_string(_videoHeight) + "px", true);
		_log("Frame rate of camera is: " + to_string(_cam.options->framerate) + "fps", true);
	}
	
	//Initiate test video file 
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{		
		//LIST FILES IN VIDEO DIRECTORY AND THEN
		//ALLOW USER TO CHOOSE THE FILE THEYD LIKE TO SEE
		string selectedVideoFile;
		
		if (_getVideoEntry(selectedVideoFile) < 0)
		{
			_log("Exiting to main loop.", true);
			return -1;
		}
		
		_vid.open(selectedVideoFile, CAP_FFMPEG);
		
		//Load frame to do analysis
		_vid >> _frame;
		
		//Reset frame position in the video
		_vid.set(CAP_PROP_POS_FRAMES, 0);
		
		//Timer's needed to ensure video is played at proper fps
		_timers["videoFrameTimer"] = _millis();
		
		//Vid FPS so i can draw properly
		_vidFPS = _vid.get(CAP_PROP_FPS);
		_vidPeriod = 1000 / _vidFPS;
		
		//Set class video width and height for tracker
		_videoWidth = _frame.size().width;
		_videoHeight = _frame.size().height;	
		
		//Keep track of frames during playback so it can be looped at last frame
		_vidNextFramePos = 0;
		_vidFramesTotal = _vid.get(CAP_PROP_FRAME_COUNT);
		
		_log("Selecting video \"" + selectedVideoFile + "\"", true);
		_log("\t>> Width: " + to_string(_videoWidth) + "px", true);
		_log("\t>> Width: " + to_string(_videoHeight) + "px", true);
		_log("\t>> Frame rate: " + to_string(_vidFPS) + "fps", true);
	}
	
	//If calibration mode, we want the mat to be a certain width/height so we can easily fit four on there
	double scaleFactorW, scaleFactorH;	
	scaleFactorW = (double) _videoWidth / MAX_WIDTH_PER_FRAME;
	scaleFactorH = (double) _videoHeight / MAX_HEIGHT_PER_FRAME;	
	if (scaleFactorH < 1)
	{
		_scaleFactor = min(scaleFactorH, scaleFactorW);
	}
	else
	{
		_scaleFactor = max(scaleFactorH, scaleFactorW);
	}	
	
	if (_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		_videoWidth /= _scaleFactor;
		_videoHeight /= _scaleFactor;		
	}
	
	//Initiate tracking
	if (_mode != fcMode::VIDEO_RECORDER)
	{
		//Initialize tracker now
		_fishTrackerObj.init(_videoWidth, _videoHeight);
		_ROIRects.clear();
	}
	
	//If needed, set tracker mode to calibration so it can return extra mats for evaluation
	if (_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		_fishTrackerObj.setMode(ftMode::CALIBRATION);
	}
	
	//Initiate calibration parameters
		
	//Turn LED light on 
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION || _mode == fcMode::VIDEO_RECORDER)
	{
		_ledState = true;
		gpioWrite(LED_PIN, _ledState);		
	}
	
	//Initiate sensors - including serial for ultrasonic
	_currentDepth = -1;
	_currentTemp = -1;
	
	//Load any saved file parameters

	//initialize frame to all black to start
	_frame = Mat::zeros(Size(_videoWidth, _videoHeight), CV_8UC3);

	return 1;
}


int FishCenS::update()
{
	
	
	if (_mode == fcMode::TRACKING)
	{					
		//Start depth sensors thread every DEPTH_PERIOD
//		Depth depthObj;
		
		if ((_millis() - _timers["tempTimer"]) >= TEMPERATURE_PERIOD)
		{	
			_timers["tempTimer"] = _millis();
			Temperature::getTemperature(_currentTemp, _baseLock);
			
			_ledState = !_ledState;
			gpioWrite(LED_PIN, _ledState);			
		}
		
//		if ((_millis() - _timers["depthTimer"]) >= DEPTH_PERIOD)
//		{	
//			_timers["depthTimer"] = _millis();	
//			
//			if (depthObj.init() > 0)
//			{				
//				depthObj.getDepth(_currentDepth, _baseLock);
//				
//				
//				//			std::thread depthThread(&Depth::getDepth, depth, ref(_currentDepth), ref(_baseLock));
//				//			_threadVector.push_back(move(depthThread));
//			}
//		
//		}
	}

	
	//	Next two if statements just load _frame with the proper content,
	//	depending on whether the camera is being read or a test video is
	//	instead being read.	
	//Read camera
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION || _mode == fcMode::VIDEO_RECORDER)
	{
		//Load video frame, if timeout, restart while loop
		if (!_cam.getVideoFrame(_frame, 1000))
		{
			_log("Timeout error\n", true);
			return -1;
		}
	}
	
	//Read video
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		if ((_millis() - _timers["videoFrameTimer"]) < _vidPeriod)
		{
			return -1;
		}
		_timers["videoFrameTimer"] = _millis();
		
		//Load frame to do analysis
		_vid >> _frame;		
		
		_vidNextFramePos++;
		
		//Loop video - Reset video to frame 0 if end of video is reached
		if (_vidNextFramePos > _vidFramesTotal)
		{
			_vidNextFramePos = 0;
			_vid.set(CAP_PROP_POS_FRAMES, 0);
		}
		
	}
	
	//Resize frames if in calibration mode so four images can sit in one easily
	if (_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		resize(_frame, _frame, Size(_videoWidth, _videoHeight));
	}
	
	//Start tracking thread
	if (_mode != fcMode::VIDEO_RECORDER)
	{
		//Remove bg, run tracker, do image processing
		std::thread trackingThread(&FishTracker::run, _fishTrackerObj, ref(_frame), ref(_returnMats), ref(_baseLock), ref(_fishCount), ref(_ROIRects));
		_threadVector.push_back(move(trackingThread));		
	}
		
	//All threads need to be joined before drawing (or looping back into update)
	for (thread & thr : _threadVector)
	{
		if (thr.joinable())
		{
			thr.join();
		}
	}
	
	return 1;
}


int FishCenS::draw()
{
	lock_guard<mutex> guard(_baseLock);
	
	if (_millis() - _timers["drawTime"] < DRAW_PERIOD)
	{
		return -1;
	}
	
	_timers["drawTime"] = _millis();
	
	//Get frameDraw prepared
	if((_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO) && !_returnMats.empty())
	{
		cvtColor(_returnMats[2].mat, _returnMats[2].mat, COLOR_GRAY2BGR);
		cvtColor(_returnMats[0].mat, _returnMats[0].mat, COLOR_GRAY2BGR);		
		hconcat(_returnMats[0].mat, _returnMats[2].mat, _returnMats[0].mat);					
		hconcat(_frame, _returnMats[1].mat, _frameDraw);		
		vconcat(_frameDraw, _returnMats[0].mat, _frameDraw);
	}
	
	if (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::VIDEO_RECORDER)
	{		
		_frame.copyTo(_frameDraw);
	}
	
	//With tracking mode, we need sensor information (Maybe need this for calibration mode w video too?)
	if (_mode == fcMode::TRACKING)
	{
		//Sensor strings to put on screen
		string depthStr = "Depth: " + to_string(_currentDepth);
		string tempStr = "Temperature: " + to_string(_currentTemp);
		
		putText(_frameDraw, depthStr, DEPTH_STRING_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		putText(_frameDraw, tempStr, TEMP_STRING_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
	}
	
	if (!_frameDraw.empty())
	{
		imshow("Video", _frameDraw);
	}
	_returnKey = waitKey(1);
	
	
	return 1;
}





void FishCenS::_trackingUpdate()
{
	
}


void FishCenS::_calibrateUpdate()
{
}


void FishCenS::_videoRecordUpdate()
{
}


int FishCenS::_getVideoEntry(string& selectionStr)
{
	//Prepare video path
	string testVideoPath = TEST_VIDEO_PATH;
	if (testVideoPath.at(testVideoPath.size() - 1) == '/')
	{
		testVideoPath.pop_back();
	}
	
	//This is the return variable
	int videoEntered = -1;
	
	//Get vector of all files
	vector<string> videoFileNames;
	for (const auto & entry : fs::directory_iterator(TEST_VIDEO_PATH))///fs::directory_iterator(TEST_VIDEO_PATH))
	{		
		videoFileNames.push_back(entry.path());
	}
	
	//Need to return and exit to main if there are no files
	if (videoFileNames.size() == 0)
	{
		_log("No video files in folder found in " + testVideoPath + "/", true);
		return -1;
	}
	
	//Variables for iterating through part of the vidoe file folder.
	int page = 0;
	int pageTotal = (videoFileNames.size() / VIDEOS_PER_PAGE) + 1;
	cout << "Page total is " << pageTotal << endl;
	
	cout << videoFileNames.size() << " files found: \n";
	
	//Shows 
	_showVideoList(videoFileNames, page);
	
	while (videoEntered == -1)
	{
		cout << "Select video. Enter \"q\" to go back. Enter \"n\" or \"p\" for more videos.\n";
		cout << "To select video, enter number associated with file number.\n";
		cout << "I.e., to select \"File 4:\t \'This video.avi\'\", you would simply enter 4.\n";
		cout << "File: ";
	
		string userInput;
		cin >> userInput;
	
		//Leave the function and go back to main
		if (userInput == "q" || userInput == "Q") 
		{
			return -1;
		}
		
		//Next page, show video files, re-do while loop
		if (userInput == "n" || userInput == "N")
		{
			if (++page >= pageTotal)
			{
				page = 0;
			}
			_showVideoList(videoFileNames, page);
			continue;
		}
		
		//Previous page, show video files, re-do while loop
		if (userInput == "p" || userInput == "P")
		{
			if (--page < 0)
			{
				page = pageTotal - 1;				
			}
			_showVideoList(videoFileNames, page);
			continue;
		}
		
		//Need to make sure the string can be converted to an integer
		bool inputIsInteger = true;
		for (int charIndex = 0; charIndex < (int) userInput.size(); charIndex++)
		{
			inputIsInteger = isdigit(userInput[charIndex]);
			
			if (!inputIsInteger)
			{
				break;
			}
		}
		
		if (!inputIsInteger) 
		{
			_log("Input must be an integer between 0 and " + to_string(videoFileNames.size() - 1) + " (inclusive)", true);
			continue;
		}
		
		int userInputInt = stoi(userInput);
		
		if (userInputInt < 0 || userInputInt >= (int) videoFileNames.size())
		{			
			_log("Input must be an integer between 0 and " + to_string(videoFileNames.size() - 1) + " (inclusive)", true);
			continue;
		}
		
		videoEntered = userInputInt;		
	}	
	
	selectionStr = videoFileNames[videoEntered];
	return 1;
}


void FishCenS::_showVideoList(vector<string> videoFileNames, int page)
{
	//Get indexing numbers
	int vecBegin = page * VIDEOS_PER_PAGE;
	int vecEnd = (page + 1) * VIDEOS_PER_PAGE;
	
	//Make sure we don't list files exceeding array size later
	if (vecEnd > (int) videoFileNames.size())
	{
		vecEnd = videoFileNames.size();
	}
	
	cout << "Showing files " << vecBegin << "-" << vecEnd << " of a total " << videoFileNames.size() << " files.\n";
	
	//Iterate and list file names
	for (int fileIndex = vecBegin; fileIndex < vecEnd; fileIndex++)
	{		
		//This gives us the full path, which is useful later, but we just need the particular file name
		//Therefore, following code delimits with the slashbars
		stringstream buffSS(videoFileNames[fileIndex]);
		string currentFile;
		
		while (getline(buffSS, currentFile, '/'))
		{
		}	
		
		cout << "\t>> File " << fileIndex << ":\t \"" << currentFile << "\"\n";
	}
}



string FishCenS::_getTime()
{
	//Create unique timestamp for folder
	stringstream timestamp;
	
	//First, create struct which contains time values
	time_t now = time(0);
	tm *ltm = localtime(&now);
	
	//Store stringstream with numbers	
	timestamp << 1900 + ltm->tm_year << "_";
	
	//Zero-pad month
	if ((1 + ltm->tm_mon) < 10) 
	{
		timestamp << "0";
	}
	
	timestamp << 1 + ltm->tm_mon << "_";
	
	//Zero-pad day
	if ((1 + ltm->tm_mday) < 10) 
	{
		timestamp << "0";
	}
	
	timestamp << ltm->tm_mday << "_";
	
	//Zero-pad hours
	if (ltm->tm_hour < 10) 
	{
		timestamp << "0";
	}
	
	timestamp << ltm->tm_hour << "h";
	
	//Zero-pad minutes
	if (ltm->tm_min < 10) 
	{
		timestamp << "0";
	}
	
	timestamp << ltm->tm_min << "m";
	
	//Zero-pad seconds
	if (ltm->tm_sec < 10) 
	{
		timestamp << "0";
	}
	
	timestamp << ltm->tm_sec << "s";
	
	//Return string version of ss
	return timestamp.str();
}


double FishCenS::_millis()
{
	return 1000 * getTickCount() / getTickFrequency();
}


void FishCenS::_log(string data, bool outputToScreen /* = false */)
{
	string dataStr = _getTime() + ":\t" + data + "\n";

	if (_fcLogger.size() > MAX_LOG_SIZE)
	{
		_fcLogger.erase(_fcLogger.begin());
	}
	
	_fcLogger.push_back(dataStr);
	
	if (outputToScreen == true)
	{
		cout << dataStr;
	}
}

int FishCenS::_saveLogFile()
{
	string loggerPathStr = LOGGER_PATH;
	
	//Add slash if folder-path does not end in slash so filename doesn't merge with the folder it's in
	if (loggerPathStr[loggerPathStr.size() - 1] != '/')
	{
		loggerPathStr += '/';
	}
	
	fs::path loggerPath = loggerPathStr;
	
	//Check if data folder exists in first place
	if (!fs::exists(loggerPath))
	{
		string file_command = "mkdir -p " + loggerPathStr;	
	
		//Create directory for saving logger file in
		if (system(file_command.c_str()) == -1)
		{
			cout << _getTime() + "WARNING: Could not save file!! Reason: Directory does not exist and cannot be created.\n";
			return -1;
		}
		
		//Change permission on all so it can be read immediately by users
		file_command = "chmod -R 777 ./" + loggerPathStr;
		system(file_command.c_str());
	}
	
	//Now open the file and dump the contents of the logger file into it
	ofstream outLogFile(loggerPathStr + _logFileName + ".txt");
	for (auto logLine : _fcLogger)
	{
		outLogFile << logLine;
	}
	outLogFile.close();
	
	return 1;
}





