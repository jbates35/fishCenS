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
#include <mutex>

namespace fs = std::filesystem;

FishCenS::FishCenS()
{
}


FishCenS::~FishCenS()
{
	_log("Closing stream...");
	destroyAllWindows();
	_cam.stopVideo();

	//Turn LED off
	gpioSetMode(LED_PIN, PI_OUTPUT);
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
		_update();
		_draw();

//		thread updateThread(&FishCenS::_updateThreadStart, this);
//		thread drawThread(&FishCenS::_drawThreadStart, this);
//
//		updateThread.join();
//		drawThread.join();
	}

	//Temporary - save log file
	_saveLogFile();

	return 1;
}

int FishCenS::init(fcMode mode)
{	
	//Testing of parameters
	_testing = false;

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
	_timers["ledTimer"] = 0;

	//Tracking and fish counting
	_fishCount = 0;
	_fishTrackerObj.init(VIDEO_WIDTH, VIDEO_HEIGHT);

	//Video stuff that needs to be initialized
	_videoRecordState = vrMode::VIDEO_OFF;
	_recordOn = false;

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
		{
			scoped_lock lock(_videoLock, _pwmLock, _baseLock, _frameLock);
			
			if (!_cam.getVideoFrame(_frame, 1000))
			{
				_log("Timeout error while initializing", true);
			}
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
		_videoWidth = _frame.size().width/2;
		_videoHeight = _frame.size().height/2;
		
		resize(_frame, _frame, Size(_videoWidth, _videoHeight));
		
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
		_gui_object.init(_fishTrackerObj);
	}

	//Initiate calibration parameters

	//Setup LED to be PWM
	_ledState = true;
	gpioSetMode(LED_PIN, PI_ALT0);
	_ledPwmFreq = DEFAULT_LED_FREQ;
	_ledPwmMin = LED_DEFAULT_PWM_MIN;
	_ledPwmMax = LED_DEFAULT_PWM_MAX;
	_ledPwmInt = LED_DEFAULT_PWM_INT;
	_ledPwmDC = (_ledPwmMin + _ledPwmMax) / 2;
	gpioHardwarePWM(LED_PIN, _ledPwmFreq, _ledPwmDC);

	//Initiate sensors - including serial for ultrasonic
	_currentDepth = -1;
	_currentTemp = -1;

	//Load any saved file parameters

	//initialize frame to all black to start
	_frame = Mat::zeros(Size(_videoWidth, _videoHeight), CV_8UC3);

	std::cout << "We've come here.";
	
	return 1;
}


int FishCenS::_update()
{

	//Set LED PWM (Will be where camera lighting -> PWM code goes)
	_setLED();

	
	if(_mode == fcMode::VIDEO_RECORDER)
	{
		if(_returnKey=='r' or _returnKey == 'R')
		{
			_recordOn = true;
			_returnKey = '\0';

			std::cout << "Record thread starting ... \n";
			
			thread recordThread(&FishCenS::_videoRunThread, this);
			recordThread.detach();
		}
	}

	if (_mode == fcMode::TRACKING)
	{
		//Start depth sensors thread every DEPTH_PERIOD
//		Depth depthObj;

		if ((_millis() - _timers["tempTimer"]) >= TEMPERATURE_PERIOD)
		{
			_timers["tempTimer"] = _millis();
			//Temperature::getTemperature(_currentTemp, _baseLock);
			thread temperatureThread(Temperature::getTemperature, ref(_currentTemp), ref(_tempLock));
			temperatureThread.detach();		
		}

		if ((_millis() - _timers["depthTimer"]) >= DEPTH_PERIOD)
		{
			_timers["depthTimer"] = _millis();

			std::thread depthThread(&Depth::run, ref(_depthObj), ref(_currentDepth), ref(_depthLock));
			depthThread.detach();

		}
	}


	//	Next two if statements just load _frame with the proper content,
	//	depending on whether the camera is being read or a test video is
	//	instead being read.
	//Read camera
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION || _mode == fcMode::VIDEO_RECORDER)
	{
		scoped_lock lock(_frameLock, _videoLock);
		
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
		scoped_lock lock(_frameLock);
		
		if ((_millis() - _timers["videoFrameTimer"]) < _vidPeriod)
		{
			return -1;
		}
		_timers["videoFrameTimer"] = _millis();

		//Load frame to do analysis
		{
			scoped_lock lock(_frameLock);
			_vid >> _frame;
			resize(_frame, _frame, Size(_videoWidth, _videoHeight));
		}
		
		
		_vidNextFramePos++;

		//Loop video - Reset video to frame 0 if end of video is reached
		if (_vidNextFramePos > _vidFramesTotal)
		{
			_vidNextFramePos = 0;
			_vid.set(CAP_PROP_POS_FRAMES, 0);
		}

	}

	//Resize frames if in calibration mode so four images can sit in one easily
	if (_mode == fcMode::CALIBRATION)// || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		scoped_lock lock(_frameLock);
		resize(_frame, _frame, Size(_videoWidth, _videoHeight));
	}

	//Start tracking thread
	if (_mode != fcMode::VIDEO_RECORDER && !_frame.empty())
	{
    //Remove bg, run tracker, do image processing
		std::thread trackingThread(&FishTracker::run, ref(_fishTrackerObj), ref(_frame), ref(_returnMats), ref(_frameLock), ref(_fishCount), ref(_ROIRects));
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


int FishCenS::_draw()
{

	if (_millis() - _timers["drawTime"] < DRAW_PERIOD)
	{
		return -1;
	}

	_timers["drawTime"] = _millis();

	//Get frameDraw prepared
	if ((_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO) && !_returnMats.empty())
	{
		scoped_lock lock(_frameLock, _frameDrawLock);
    
		//Show info about tracked objects
		_showRectInfo(_returnMats[0].mat);
		
		for (auto matsStruct : _returnMats)
		{
			putText(matsStruct.mat, "Mat: " + matsStruct.title, Point(5, 15), FONT_HERSHEY_PLAIN, 0.6, Scalar(155, 230, 255), 1);	
		}
		
		string fishCountStr = "Fish counted: " + to_string(_fishCount);
		string fishTrackedStr = "Fish currently tracked: " + to_string(_ROIRects.size());
		putText(_returnMats[0].mat, fishCountStr, FISH_COUNT_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		putText(_returnMats[0].mat, fishTrackedStr, FISH_TRACKED_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		
		//Change color spaces of mat to fit BGR
		cvtColor(_returnMats[3].mat, _returnMats[3].mat, COLOR_GRAY2BGR);
		cvtColor(_returnMats[1].mat, _returnMats[1].mat, COLOR_GRAY2BGR);

		//Create a side by side image of 4 shots
		hconcat(_returnMats[1].mat, _returnMats[3].mat, _returnMats[1].mat);
		hconcat(_returnMats[0].mat, _returnMats[2].mat, _returnMats[0].mat);
		vconcat(_returnMats[0].mat, _returnMats[1].mat, _frameDraw);
			
		//Update with trackbars
		_gui_object._gui(_frameDraw, _fishTrackerObj);		
	}

	if (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::VIDEO_RECORDER)
	{
		scoped_lock lock(_frameDrawLock);

		if (_returnMats.size() > 0) 
		{
			_returnMats[0].mat.copyTo(_frameDraw);			
		}
		else
		{
			_frame.copyTo(_frameDraw);
		}
		
		if (_mode != fcMode::VIDEO_RECORDER) 
		{		
			//Show info about tracked objects
			_showRectInfo(_frameDraw);			
		}
			
	}

	//With tracking mode, we need sensor information (Maybe need this for calibration mode w video too?)
	if (_mode == fcMode::TRACKING)
	{
		//Sensor strings to put on screen
		{
			//Lock depth lock guard
			//lock_guard<mutex> guard(_depthLock);
			string depthStr = "Depth: " + to_string(_currentDepth);
			putText(_frameDraw, depthStr, DEPTH_STRING_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		}
		
		{
			//Lock temp lock guard
			//lock_guard<mutex> guard(_tempLock);
			string tempStr = "Temperature: " + to_string(_currentTemp);
			putText(_frameDraw, tempStr, TEMP_STRING_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		}
	}
  
	if (!_frameDraw.empty() && (_mode != fcMode::CALIBRATION && _mode != fcMode::CALIBRATION_WITH_VIDEO))
	{
		scoped_lock lock(_frameDrawLock);
		imshow("Video", _frameDraw);
	}
	_returnKey = waitKey(1);

	if(_returnKey == 's' || _returnKey == 'S')
	{
		_recordOn = false;
	}


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

void FishCenS::_videoRun()
{
	double vidTimer = getTickCount()/getTickFrequency();

//init(Mat& frame, mutex& lock, double fps, string filePath /* = NULL */)
	_vidRecord.init(_frame, _videoLock, 30);

	while(_recordOn)
	{		
		if(getTickCount()/getTickFrequency() - vidTimer >= 1/30)
		{	
			scoped_lock lockGuard(_videoLock);
			vidTimer = getTickCount()/getTickFrequency();
			_vidRecord.run(_frame, _videoLock);
			cout << "Record going..." << endl;
		}
	}

	_vidRecord.close();
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



void FishCenS::_updateThreadStart(FishCenS* ptr)
{
	ptr->_update();
}


void FishCenS::_drawThreadStart(FishCenS* ptr)
{
	ptr->_draw();
}

void FishCenS::_videoRunThread(FishCenS* ptr)
{
	ptr->_videoRun();
}

void FishCenS::setTesting(bool isTesting)
{
	_testing = isTesting;
	
	//Set classes as well
	_fishTrackerObj.setTesting(_testing);
}

//Shows info such as ROI, midline, and text about tracked object info
int FishCenS::_showRectInfo(Mat& im)
{
	if (im.empty())
	{
		return false;
	}
	
	//Fish information
	string fishCountStr = "Fish counted: " + to_string(_fishCount);
	string fishTrackedStr = "Fish currently tracked: " + to_string(_ROIRects.size());
	putText(im, fishCountStr, FISH_COUNT_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
	putText(im, fishTrackedStr, FISH_TRACKED_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
	
	//Fish ROIs drawn + other information
	int index = 0;
	for (Rect fishROI : _ROIRects)
	{							
		rectangle(im, fishROI, Scalar(130, 0, 180), 2);
					
		vector<String> rectString;
				
		rectString.push_back("Object " + to_string(index++) + " tracked");
		rectString.push_back("Area: " + to_string(fishROI.width * fishROI.height));		
		rectString.push_back("Point: <" + to_string(fishROI.x) + ", " + to_string(fishROI.y) + ">");					
		Point textPoint = Point(fishROI.x + 10, fishROI.y + fishROI.height + 10);
				
		for (auto i = 0; i < (int) rectString.size(); i++)
		{
			putText(im, rectString[i], textPoint, FONT_HERSHEY_PLAIN, 0.7, Scalar(130, 0, 180), 1);
			textPoint.y += 10;
		}
	}	
			
	//Draw line for seeing middle
	int midPoint = _fishTrackerObj.getFrameCenter();
	line(im, Point(midPoint, 0), Point(midPoint, im.size().height), Scalar(0, 255, 255), 2);
	
	return true;
}

//Sets up PWM by seeing what amoutn of lightis in the cmaera and calibrates
void FishCenS::_setLED()
{
	Mat lightFrame;
	
	if (_millis() - _timers["ledTimer"] <= LIGHT_REFRESH) 
	{
		return;
	}
	
	_timers["ledTimer"] = _millis();
		
	//Store the class's camera object into lightFrame
	{
		scoped_lock guard (_pwmLock);

		gpioHardwarePWM(LED_PIN, _ledPwmFreq, 0);
		
		double thisTimer = _millis();

		while(_millis() - thisTimer <= 300) 
		{
		}

		if (!_cam.getVideoFrame(lightFrame, 1000))
		{
			_log("Timeout error while initializing", true);
		}

		thisTimer = _millis();

		while(_millis() - thisTimer <= 200) 
		{
		}
	}

	// Lock frame to check lighing 
	{	
		if(!lightFrame.empty())
		{
			cv::cvtColor(lightFrame, lightFrame, cv::COLOR_RGB2GRAY);
			// Stores the mean of the channels, since all gray only index 0 has value
			Scalar gray_sc = mean(lightFrame);
			
			//Calculate the amount of light in the frame
			int lightAmt =gray_sc[0];
			
			lightAmt = sqrt(lightAmt) * 16;

			//std::cout << "Light amount is " << lightAmt << "\n";
			
			//Map 255-lightAmt to LED_DEFAULT_PWM_MIN - LED_DEFAULT_PWM_MAX
			_ledPwmDC = (255 - lightAmt) * (LED_DEFAULT_PWM_MAX - LED_DEFAULT_PWM_MIN) / 255 + LED_DEFAULT_PWM_MIN;
		
			//Testing
			std::cout << "Success. LED PWM Value is " << _ledPwmDC << "\n";
		}
		else{
			_log("No data in frame");
			std::cout << "Not success. LED PWM Value is " << _ledPwmDC << "\n";
		}

		//Set the PWM value of the gpioPin to pwmVal
		gpioHardwarePWM(LED_PIN, _ledPwmFreq, _ledPwmDC);
	}
		


}