#include "fishCenS.h"

#include "misc/fcFuncs.h"

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
using namespace _fc;

FishCenS::FishCenS()
{	
	_currentDate = _fcfuncs::getDate();
	_picFolderPath = PIC_BASE_PATH + _currentDate + "/";

	//Create directories if they don't exist and set permissions so files can be written to them later
	vector<string> _directoryNames = {
		LOGGER_PATH,
		VIDEO_PATH,
		PIC_BASE_PATH,
		_picFolderPath
	};

	for(auto x : _directoryNames)
	{
		if(!fs::exists(x))
		{
			fs::create_directory(x);
			fs::permissions(x, fs::perms::all);
		}
	}
}

FishCenS::~FishCenS()
{
	_fcfuncs::writeLog(_fcLogger, "Closing stream...");
	destroyAllWindows();
	_cam.stopVideo();

	//Turn LED off
	gpioSetMode(LED_PIN, PI_OUTPUT);
	gpioWrite(LED_PIN, 0);

	_fcfuncs::saveLogFile(_fcLogger, _logFileName, LOGGER_PATH);
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

		// thread updateThread(&FishCenS::_updateThreadStart, this);
		// thread drawThread(&FishCenS::_drawThreadStart, this);

		// updateThread.join();
		// drawThread.join();
	}

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
	_logFileName = _fcfuncs::getTimeStamp();

	//Clear timers and load start timer
	_timers.clear();
	_timers["runTime"] = _fcfuncs::millis();
	_timers["drawTime"] = _fcfuncs::millis();
	_timers["sensorTimer"] = _fcfuncs::millis();
	_timers["ledTimer"] = 0;

	//Tracking and fish counting
	_fishCount = 0;
	_fishCountPrev = 0;
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
		_fcfuncs::writeLog(_fcLogger, "Attempting to start camera. Sleeping for " + to_string(SLEEP_TIMER) + "ms", true);
		double camTimer = _fcfuncs::millis();
		while((_fcfuncs::millis() - camTimer) < SLEEP_TIMER)
		{
		}

		//Load video frame, if timeout, restart while loop
		{
			scoped_lock lock(_videoLock, _pwmLock, _baseLock, _frameLock);
			
			if (!_cam.getVideoFrame(_frame, 1000))
			{
				_fcfuncs::writeLog(_fcLogger, "Timeout error while initializing", true);
			}
		}

		//If there's nothing in the video frame, also exit
		if (_frame.empty())
		{
			_fcfuncs::writeLog(_fcLogger, "Camera doesn't work, frame empty", true);
			return -1;
		}

		//Set class video width and height for tracker
		_videoWidth = _frame.size().width * VIDEO_SCALE_FACTOR;
		_videoHeight = _frame.size().height * VIDEO_SCALE_FACTOR;

		_fcfuncs::writeLog(_fcLogger, "Camera found. Size of camera is: ", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Width: " + to_string(_videoWidth) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Height: " + to_string(_videoHeight) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "Frame rate of camera is: " + to_string(_cam.options->framerate) + "fps", true);
	}

	//Initiate test video file
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		//LIST FILES IN VIDEO DIRECTORY AND THEN
		//ALLOW USER TO CHOOSE THE FILE THEYD LIKE TO SEE
		string selectedVideoFile;

		if (_fcfuncs::getVideoEntry(selectedVideoFile, VIDEO_PATH) < 0)
		{
			_fcfuncs::writeLog(_fcLogger, "Exiting to main loop.", true);
			return -1;
		}

		_vid.open(selectedVideoFile, CAP_FFMPEG);

		//Load frame to do analysis
		_vid >> _frame;

		//Reset frame position in the video
		_vid.set(CAP_PROP_POS_FRAMES, 0);

		//Timer's needed to ensure video is played at proper fps
		_timers["videoFrameTimer"] = _fcfuncs::millis();

		//Vid FPS so i can draw properly
		_vidFPS = _vid.get(CAP_PROP_FPS);
		_vidPeriod = 1000 / _vidFPS;

		//Set class video width and height for tracker
		_videoWidth = _frame.size().width * VIDEO_SCALE_FACTOR;
		_videoHeight = _frame.size().height * VIDEO_SCALE_FACTOR;
		
		resize(_frame, _frame, Size(_videoWidth, _videoHeight));
		
		//Keep track of frames during playback so it can be looped at last frame
		_vidNextFramePos = 0;
		_vidFramesTotal = _vid.get(CAP_PROP_FRAME_COUNT);

		_fcfuncs::writeLog(_fcLogger, "Selecting video \"" + selectedVideoFile + "\"", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Width: " + to_string(_videoWidth) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Height: " + to_string(_videoHeight) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Frame rate: " + to_string(_vidFPS) + "fps", true);
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

	//Load sql database
	if(_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO)
	{
		_sqlObj.init();
	}

	return 1;
}


int FishCenS::_update()
{

	//Set LED PWM (Will be where camera lighting -> PWM code goes)
	if(_mode == fcMode::TRACKING)
	{
		_setLED();
	}
	
	if(_mode == fcMode::VIDEO_RECORDER && (_returnKey=='r' or _returnKey == 'R'))
	{
		_recordOn = true;
		_returnKey = '\0';

		std::cout << "Record thread starting ... \n";
		
		thread recordThread(&FishCenS::_videoRunThread, this);
		recordThread.detach();
	}

	//Start sensor stuff
	if (_mode == fcMode::TRACKING && (_fcfuncs::millis() - _timers["sensorsTimer"]) >= SENSORS_PERIOD)
	{
		//Update sensor data
		_timers["sensorsTimer"] = _fcfuncs::millis();

		std::thread sensorThread(&FishCenS::_sensorsThread, this);
		sensorThread.detach();
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
			_fcfuncs::writeLog(_fcLogger, "Timeout error\n", true);
			return -1;
		}
	}

	//Read video
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		if ((_fcfuncs::millis() - _timers["videoFrameTimer"]) < _vidPeriod)
		{
			return -1;
		}
		_timers["videoFrameTimer"] = _fcfuncs::millis();

		//Load frame to do analysis
		{
			scoped_lock lock(_frameLock);
			_vid >> _frame;
		}
		
		
		_vidNextFramePos++;

		//Loop video - Reset video to frame 0 if end of video is reached
		if (_vidNextFramePos >= _vidFramesTotal)
		{
			_vidNextFramePos = 0;
			_vid.set(CAP_PROP_POS_FRAMES, 0);
		}
	}

	//Resize frame
	if(_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO)
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
	
	if (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO)
	{
		//Check to make sure we still have the correct date
		if (_currentDate != _fcfuncs::getDate())
		{
			_currentDate = _fcfuncs::getDate();
			_picFolderPath = PIC_BASE_PATH + _currentDate + "/";
	
			if (!fs::exists(_picFolderPath))
			{
				fs::create_directory(_picFolderPath);
				fs::permissions(_picFolderPath, fs::perms::all);		
			}	
		}	
	
		if (_fishCount != _fishCountPrev)
		{
			//Get current time for recording picture
			string picTime = _fcfuncs::getTimeStamp();
			string filename = _picFolderPath + picTime + ".jpg";
		
			//Save the frame as a picture in the current date folder
			imwrite(filename, _frame);
		
			_fishCountPrev = _fishCount;	

			string sqlDate, sqlTime;

			//Get current date and time for sql database
			_fcfuncs::parseDateTime(picTime, sqlDate, sqlTime);

			//Save to sql database
			_fcDatabase::fishCounterData counterData;
			counterData.date = sqlDate;
			counterData.time = sqlTime;
			counterData.count = _fishCount;
			counterData.direction = 'R';
			counterData.filename = filename;

			_sqlObj.insertFishCounterData(counterData);
		}	
	}

	return 1;
}

int FishCenS::_draw()
{

	if (_fcfuncs::millis() - _timers["drawTime"] < DRAW_PERIOD)
	{
		return -1;
	}

	_timers["drawTime"] = _fcfuncs::millis();

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

void FishCenS::_videoRun()
{
	double vidTimer = _fcfuncs::millis();
	int frameCount = 0;
	
	_vidRecord.init(_frame, _videoLock, 30);
	cout << "Video thread starting..." << endl;
	
	while(_recordOn)
	{		
		if (_fcfuncs::millis() - vidTimer >= 1000 / 30) 
		{	
			if (!_vidRecord.isInRunFunc())
			{				
				cout << "Video record frame " << frameCount++ << " being recorded. \t Frame time: " << _fcfuncs::millis() - vidTimer << "ms" << '\r';
				vidTimer = _fcfuncs::millis();				
				_vidRecord.run(_frame, _videoLock);	
			}
		}
	}

	_vidRecord.close();
	cout << "Video thread closing..." << endl;
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

void FishCenS::_sensors()
{
	//First run the sensors
	std::thread temperatureThread(Temperature::getTemperature, ref(_currentTemp), ref(_tempLock));	
	std::thread depthThread(&Depth::run, ref(_depthObj), ref(_currentDepth), ref(_depthLock));

	temperatureThread.join();	
	depthThread.join();

	//Now update SQL table
	_fcDatabase::sensorEntry data;

	//Get time and date
	String sensorDate, sensorTime;
	String sensorTimeStamp = _fcfuncs::getTimeStamp();
	_fcfuncs::parseDateTime(sensorTimeStamp, sensorDate, sensorTime);

	//Load data so we can put it into a query
	data.date = sensorDate;
	data.time = sensorTime;
	data.depth = _currentDepth;
	data.temperature = _currentTemp;

	//Insert data into SQL table
	_sqlObj.insertSensorData(data);
}

void FishCenS::_sensorsThread(FishCenS *ptr)
{
	ptr->_sensors();
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
	
	if (_fcfuncs::millis() - _timers["ledTimer"] <= LIGHT_REFRESH) 
	{
		return;
	}
	
	_timers["ledTimer"] = _fcfuncs::millis();
		
	//Store the class's camera object into lightFrame
	{
		scoped_lock guard (_pwmLock);

		gpioHardwarePWM(LED_PIN, _ledPwmFreq, 0);
		
		double thisTimer = _fcfuncs::millis();

		while(_fcfuncs::millis() - thisTimer <= 300) 
		{
		}

		if (!_cam.getVideoFrame(lightFrame, 1000))
		{
			_fcfuncs::writeLog(_fcLogger, "Timeout error while initializing", true);
		}

		thisTimer = _fcfuncs::millis();

		while(_fcfuncs::millis() - thisTimer <= 200) 
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
		else
		{
			_fcfuncs::writeLog(_fcLogger, "No data in frame");
			std::cout << "Not success. LED PWM Value is " << _ledPwmDC << "\n";
		}

		//Set the PWM value of the gpioPin to pwmVal
		gpioHardwarePWM(LED_PIN, _ledPwmFreq, _ledPwmDC);
	}
}