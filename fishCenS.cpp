#include "fishCenS.h"

#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <pigpio.h>
#include <string>
#include <dirent.h> //Probably don't need this with filesystem
#include <filesystem>

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
	destroyAllWindows();
	_cam.stopVideo();
	gpioWrite(LED_PIN, 0);
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
	
	//Tracking and fish counting
	_fishCount = 0;
	_fishTrackerObj.init(VIDEO_WIDTH, VIDEO_HEIGHT);
	
	//Video stuff that needs to be initialized
	_videoRecordState = vrMode::VIDEO_OFF;
	
	//initiate camera	
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION || _mode == fcMode::VIDEO_RECORDER)
	{
		_cam.options->video_width = VIDEO_WIDTH;
		_cam.options->video_height = VIDEO_HEIGHT;
		_cam.options->verbose = 1;
		_cam.options->list_cameras = true;
		_cam.options->framerate = 100;
		_cam.startVideo();
			
		//Load video frame, if timeout, restart while loop
		if (!_cam.getVideoFrame(_frame, 1000))
		{
			_log("Timeout error, exiting program", true);
			return -1;
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
	}
	
	//Initiate test video file 
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		//TODO: 
		//LIST FILES IN VIDEO DIRECTORY AND THEN
		//ALLOW USER TO CHOOSE THE FILE THEYD LIKE TO SEE
		
		_vid.open(TEST_VIDEO_PATH + TEST_VIDEO_FILE, CAP_FFMPEG);
		
		//Load frame to do analysis
		_vid >> _frame;
		
		//Reset frame counter
		_vid.set(CAP_PROP_POS_FRAMES, 0);
		
		//Set class video width and height for tracker
		_videoWidth = _frame.size().width;
		_videoHeight = _frame.size().height;	
		
		//Keep track of frames during playback so it can be looped at last frame
		_vidNextFramePos = 0;
		_vidFramesTotal = _vid.get(CAP_PROP_FRAME_COUNT);
	}
	
	//Initiate tracking
	if (_mode != fcMode::VIDEO_RECORDER)
	{
		//Initialize tracker now
		_fishTrackerObj.init(VIDEO_WIDTH, VIDEO_HEIGHT);
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
		gpioWrite(LED_PIN, 1);
		
	}
	
	//Initiate any sensor stuff
	
	//Load any saved file parameters

	return 1;
}


int FishCenS::update()
{
	
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
	
	switch (_mode)
	{
	case fcMode::TRACKING:
		_trackingUpdate();
		break;
	
	case fcMode::CALIBRATION:
		_calibrateUpdate();
		break;
		
	case fcMode::TRACKING_WITH_VIDEO:
		_trackingUpdate();
		break;
		
	case fcMode::CALIBRATION_WITH_VIDEO:
		_calibrateUpdate();
		break;
		
	case fcMode::VIDEO_RECORDER:
		_videoRecordUpdate();
		break;		
	}
	
	return 1;
}


int FishCenS::draw()
{
	if (_millis() - _timers["drawTime"] < DRAW_PERIOD)
	{
		return -1;
	}
	
	_timers["drawTime"] = _millis();	
	imshow("Video", _frame);
	_returnKey = waitKey(1);
	
}


int FishCenS::run()
{
	while (_returnKey != 27)
	{
		update();
		draw();
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
	if (_fcLogger.size() > MAX_LOG_SIZE)
	{
		_fcLogger.erase(_fcLogger.begin());
	}
	
	_fcLogger.push_back(_getTime() + data + "\n");
	
	if (outputToScreen == true)
	{
		cout << _getTime() + data + "\n";
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


