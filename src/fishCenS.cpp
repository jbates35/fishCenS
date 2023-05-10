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

	// Create directories if they don't exist and set permissions so files can be written to them later
	vector<string> _directoryNames = {
		LOGGER_PATH,
		VIDEO_PATH,
		PIC_BASE_PATH,
		_picFolderPath};

	for (auto x : _directoryNames)
	{
		if (!fs::exists(x))
		{
			fs::create_directory(x);
			fs::permissions(x, fs::perms::all);
		}
	}

	// Testing of parameters
	_testing = false;
	_displayOn = false;
	_sensorsOff = false;
	_ledOff = false;
	_pipelineOff = false;

}

FishCenS::~FishCenS()
{
	_fcfuncs::writeLog(_fcLogger, "Closing stream...");
	destroyAllWindows();
	_cam.stopVideo();

	if(!_pipelineOff)
		_fishPipe.close();

	// Turn LED off
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

		// Set LED PWM (Will be where camera lighting -> PWM code goes)
		if (_mode == fcMode::TRACKING)
		{
			_setLED();
		}

		_manageVideoRecord();

		// Start sensor stuff
		if (!_sensorsOff && (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO) && ((_fcfuncs::millis() - _timers["sensorsTimer"]) >= SENSORS_PERIOD))
		{
			// Update sensor data
			_timers["sensorsTimer"] = _fcfuncs::millis();

			std::thread sensorThread(&FishCenS::_sensorsThread, this);
			sensorThread.detach();
		}

		// Get the frame
		if ((_fcfuncs::millis() - _timers["camTimer"]) >= _camPeriod && (_mode != fcMode::VIDEO_RECORDER))
		{
			_timers["camTimer"] = _fcfuncs::millis();

			std::thread frameThread(&FishCenS::_loadFrameThread, this);
			frameThread.detach();
		}

		// Object detection and tracking
		if ((_fcfuncs::millis() - _timers["trackerTimer"] >= TRACKING_TIMER) && (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO))
		{
			_timers["trackerTimer"] = _fcfuncs::millis();

			// Start tracker thread
			std::thread trackerThread(&FishCenS::_trackerUpdateThread, this);
			trackerThread.detach();

			// Start machine learning thread here
			std::thread mlThread(&FishCenS::_MLUpdateThread, this);
			mlThread.detach();
		}

		if ((_fcfuncs::millis() - _timers["drawTimer"]) >= DRAW_PERIOD)
		{
			_timers["drawTimer"] = _fcfuncs::millis();

			thread drawThread(&FishCenS::_drawThreadStart, this);
			drawThread.detach();
		}
	}
	return 1;
}

int FishCenS::init(fcMode mode)
{
	// Mode of fishCenS object
	_mode = mode;

	// Return key from waitKey
	_returnKey = '\0';

	// Load filename and filepath
	_logFileName = _fcfuncs::getTimeStamp();

	// Clear timers and load start timer
	_timers.clear();
	_timers["runTime"] = _fcfuncs::millis();
	_timers["drawTimer"] = _fcfuncs::millis();
	_timers["sensorTimer"] = _fcfuncs::millis();
	_timers["ledTimer"] = 0;
	_timers["trackerTimer"] = _fcfuncs::millis();
	_timers["camTimer"] = _fcfuncs::millis();
	_timers["videoFrameTimer"] = _fcfuncs::millis();
	_timers["pipeline"] = _fcfuncs::millis();

	// Tracking and fish counting
	_fishIncremented = 0;
	_fishIncrementedPrev = 0;
	_fishDecremented = 0;
	_fishDecrementedPrev = 0;

	// Video stuff that needs to be initialized
	_videoRecordState = vrMode::VIDEO_OFF;
	_recordOn = false;

	// Make sure no threads
	_threadVector.clear();

	// initiate camera
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION || _mode == fcMode::VIDEO_RECORDER)
	{
		// FPS
		_camFPS = CAM_FPS;
		_camPeriod = 1000 / _camFPS; // in millis

		_cam.options->photo_width = VIDEO_WIDTH;
		_cam.options->photo_height = VIDEO_HEIGHT;
		_cam.options->video_width = VIDEO_WIDTH;
		_cam.options->video_height = VIDEO_HEIGHT;
		_cam.options->verbose = 1;
		_cam.options->list_cameras = true;
		_cam.options->framerate = _camFPS;
		_cam.startVideo();

		// Allow camera time to figure itself out
		_fcfuncs::writeLog(_fcLogger, "Attempting to start camera. Sleeping for " + to_string(SLEEP_TIMER) + "ms", true);
		double camTimer = _fcfuncs::millis();
		while ((_fcfuncs::millis() - camTimer) < SLEEP_TIMER)
		{
		}

		// Load video frame, if timeout, restart while loop
		if (!_cam.getVideoFrame(_frame, 1000))
		{
			_fcfuncs::writeLog(_fcLogger, "Timeout error while initializing", true);
		}

		// If there's nothing in the video frame, also exit
		if (_frame.empty())
		{
			_fcfuncs::writeLog(_fcLogger, "Camera doesn't work, frame empty", true);
			return -1;
		}

		// Set class video width and height for tracker
		_videoWidth = _frame.size().width * VIDEO_SCALE_FACTOR;
		_videoHeight = _frame.size().height * VIDEO_SCALE_FACTOR;

		_frameSize = Size(_videoWidth, _videoHeight);

		_fcfuncs::writeLog(_fcLogger, "Camera found. Size of camera is: ", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Width: " + to_string(_videoWidth) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Height: " + to_string(_videoHeight) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "Frame rate of camera is: " + to_string(_cam.options->framerate) + "fps", true);
	}

	// Initiate test video file
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		// LIST FILES IN VIDEO DIRECTORY AND THEN
		// ALLOW USER TO CHOOSE THE FILE THEYD LIKE TO SEE
		string selectedVideoFile;

		if (_fcfuncs::getVideoEntry(selectedVideoFile, VIDEO_PATH) < 0)
		{
			_fcfuncs::writeLog(_fcLogger, "Exiting to main loop.", true);
			return -1;
		}

		_vid.open(selectedVideoFile, CAP_FFMPEG);

		// Load frame to do analysis
		_vid >> _frame;

		// Reset frame position in the video
		_vid.set(CAP_PROP_POS_FRAMES, 0);

		// Timer's needed to ensure video is played at proper fps
		_timers["videoFrameTimer"] = _fcfuncs::millis();

		// Vid FPS so i can draw properly
		_vidFPS = _vid.get(CAP_PROP_FPS);
		_vidPeriod = 1000 / _vidFPS;

		// Set class video width and height for tracker
		_videoWidth = _frame.size().width * VIDEO_SCALE_FACTOR;
		_videoHeight = _frame.size().height * VIDEO_SCALE_FACTOR;
		_frameSize = Size(_videoWidth, _videoHeight);
		//resize(_frame, _frame, _frameSize);

		// Keep track of frames during playback so it can be looped at last frame
		_vidNextFramePos = 0;
		_vidFramesTotal = _vid.get(CAP_PROP_FRAME_COUNT);

		_fcfuncs::writeLog(_fcLogger, "Selecting video \"" + selectedVideoFile + "\"", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Width: " + to_string(_videoWidth) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Height: " + to_string(_videoHeight) + "px", true);
		_fcfuncs::writeLog(_fcLogger, "\t>> Frame rate: " + to_string(_vidFPS) + "fps", true);
	}

	// If calibration mode, we want the mat to be a certain width/height so we can easily fit four on there
	double scaleFactorW, scaleFactorH;
	scaleFactorW = (double)_videoWidth / MAX_WIDTH_PER_FRAME;
	scaleFactorH = (double)_videoHeight / MAX_HEIGHT_PER_FRAME;
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

	// Initiate tracking and machine learning
	if (_mode != fcMode::VIDEO_RECORDER)
	{
		// Load machine learning data
		if (_fishMLWrapper.init() < 0)
		{
			_fcfuncs::writeLog(_fcLogger, "Error loading machine learning data", true);
			return -1;
		}

		// Initialize tracker now
		if (_fishTrackerObj.init(Size(_frameSize.width * TRACKER_SCALE_FACTOR, _frameSize.height * TRACKER_SCALE_FACTOR)) < 0)
		{
			_fcfuncs::writeLog(_fcLogger, "Error initializing tracker", true);
			return -1;
		}

		_trackedData.clear();
	}

	// If needed, set tracker mode to calibration so it can return extra mats for evaluation
	if (_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		_fishTrackerObj.setMode(ftMode::CALIBRATION);
		_gui_object.init(_fishTrackerObj);
	}

	// Initiate calibration parameters

	// Setup LED to be PWM
	_ledState = true;
	gpioSetMode(LED_PIN, PI_ALT0);
	_ledPwmFreq = DEFAULT_LED_FREQ;
	_ledPwmMin = LED_DEFAULT_PWM_MIN;
	_ledPwmMax = LED_DEFAULT_PWM_MAX;
	_ledPwmInt = LED_DEFAULT_PWM_INT;
	_ledPwmDC = (_ledPwmMin + _ledPwmMax) / 2;
	gpioHardwarePWM(LED_PIN, _ledPwmFreq, _ledPwmDC);

	// Initiate sensors - including serial for ultrasonic
	_currentDepth = -1;
	_currentTemp = -1;

	// gpioSetMode(14, PI_OUTPUT);
	// gpioSetMode(15, PI_INPUT);

	//_depthObj = std::make_unique<Depth>();
	//_depthOpen = _depthObj->init();

	// Load any saved file parameters

	// initialize frame to all black to start
	_frame = Mat::zeros(Size(_videoWidth, _videoHeight), CV_8UC3);

	// Load sql database
	if (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO)
	{
		_sqlObj.init();
	}

	//Make pipeline
	if(!_pipelineOff)
		_fishPipe.init();

	return 1;
}

int FishCenS::_draw()
{

	std::unique_lock<std::mutex> singleLock(_singletonDraw, std::try_to_lock);

	if (!singleLock.owns_lock())
	{
		return 0;
	}


	// If display is off, just return - But we need the named window for the video record
	// so that the key can be caught and the video can start to be recorded
	// Also, if it's in calibration, we need that display anyway
	if (!_displayOn && !(_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO))
	{
		if (_mode == fcMode::VIDEO_RECORDER)
		{
			namedWindow("_BLANK", WINDOW_NORMAL);
			_returnKey = waitKey(1);
		}

		return 0;
	}

	// Get frameDraw prepared
	if ((_mode == fcMode::CALIBRATION || _mode == fcMode::CALIBRATION_WITH_VIDEO) && !_returnMats.empty())
	{
		scoped_lock lock(_frameLock, _frameDrawLock);

		// Show info about tracked objects
		_showRectInfo(_returnMats[0].mat);

		for (auto matsStruct : _returnMats)
		{
			putText(matsStruct.mat, "Mat: " + matsStruct.title, Point(5, 15), FONT_HERSHEY_PLAIN, 0.9, Scalar(255, 190, 100), 1);
		}

		string fishCountIncStr = "Fish -> upstream: " + to_string(_fishIncremented);
		string fishCountDecstr = "Fish -> downstream: " + to_string(_fishDecremented);

		putText(_returnMats[0].mat, fishCountIncStr, FISH_INC_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		putText(_returnMats[0].mat, fishCountDecstr, FISH_DEC_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);

		// Change color spaces of mat to fit BGR
		cvtColor(_returnMats[3].mat, _returnMats[3].mat, COLOR_GRAY2BGR);
		cvtColor(_returnMats[1].mat, _returnMats[1].mat, COLOR_GRAY2BGR);

		// Create a side by side image of 4 shots
		hconcat(_returnMats[1].mat, _returnMats[3].mat, _returnMats[1].mat);
		hconcat(_returnMats[0].mat, _returnMats[2].mat, _returnMats[0].mat);
		vconcat(_returnMats[0].mat, _returnMats[1].mat, _frameDraw);

		// Update with trackbars
		_gui_object._gui(_frameDraw, _fishTrackerObj);
	}

	// Get a local copy of the frame for concurrency
	Mat localFrame;

	if (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::VIDEO_RECORDER)
	{
		{
			scoped_lock lock(_frameLock);

			if (_frame.empty())
			{
				cout << "Frame is empty" << endl;
				return -1;
			}

			localFrame = _frame.clone();
		}

		if (_mode != fcMode::VIDEO_RECORDER)
		{
			// Show info about tracked objects
			_showRectInfo(localFrame);
		}
	}

	// With tracking mode, we need sensor information (Maybe need this for calibration mode w video too?)
	if (_mode == fcMode::TRACKING && !_sensorsOff)
	{
		// Sensor strings to put on screen
		{
			// Lock depth lock guard
			string depthStr = "Depth: " + to_string(_currentDepth);
			putText(localFrame, depthStr, DEPTH_STRING_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		}

		{
			// Lock temp lock guard
			// lock_guard<mutex> guard(_tempLock);
			string tempStr = "Temperature: " + to_string(_currentTemp);
			putText(localFrame, tempStr, TEMP_STRING_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, YELLOW, SENSOR_STRING_THICKNESS);
		}
	}

	if (!localFrame.empty() && (_mode != fcMode::CALIBRATION && _mode != fcMode::CALIBRATION_WITH_VIDEO))
	{
		imshow("Video", localFrame);
	}

	_returnKey = waitKey(1);

	return 1;
}

void FishCenS::_videoRun()
{
	double vidTimer = _fcfuncs::millis();
	int frameCount = 0;

	_vidRecord.init(_frame, _videoLock, 30);
	cout << "Video thread starting..." << endl;

	while (_recordOn)
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

void FishCenS::_drawThreadStart(FishCenS *ptr)
{
	ptr->_draw();
}

void FishCenS::_videoRunThread(FishCenS *ptr)
{
	ptr->_videoRun();
}

void FishCenS::_sensors()
{
	cout << "Thread starting..." << endl;

	unique_lock<mutex> singleLock(_sensorsLock, try_to_lock);

	// First run the sensors
	std::thread temperatureThread(Temperature::getTemperature, ref(_currentTemp), ref(_tempLock));

	unique_ptr<Depth> depthObj = std::make_unique<Depth>();

	if (depthObj->init() >= 0)
	{
		depthObj->getDepth(_currentDepth, _depthLock);
	}
	// if(_depthOpen<0)
	// {
	// 	_depthOpen = _depthObj->init();
	// }
	// else
	// {
	// 	_depthObj -> getDepth(_currentDepth, _depthLock);
	// 	// std::thread depthThread(&Depth::run, ref(*depthObj), ref(_currentDepth), ref(_depthLock));
	//     // depthThread.join();
	// }

	temperatureThread.join();

	// Now update SQL table
	_fcDatabase::sensorEntry data;

	// Get time and date
	String sensorDate, sensorTime;
	String sensorTimeStamp = _fcfuncs::getTimeStamp();
	_fcfuncs::parseDateTime(sensorTimeStamp, sensorDate, sensorTime);

	// Load data so we can put it into a query
	data.date = sensorDate;
	data.time = sensorTime;
	data.depth = _currentDepth;
	data.temperature = _currentTemp;

	// Insert data into SQL table
	scoped_lock sqlLock(_sqlLock);
	_sqlObj.insertSensorData(data);
}

void FishCenS::_sensorsThread(FishCenS *ptr)
{
	ptr->_sensors();
}

void FishCenS::setTesting(bool isTesting)
{
	_testing = isTesting;

	// Set classes as well
	_fishTrackerObj.setTesting(_testing);
}

// Shows info such as ROI, midline, and text about tracked object info
int FishCenS::_showRectInfo(Mat &im)
{
	if (im.empty())
	{
		return false;
	}

	// Fish information
	string fishCountIncStr = "Fish -> upstream: " + to_string(_fishIncremented);
	string fishCountDecstr = "Fish -> downstream: " + to_string(_fishDecremented);


	putText(im, fishCountIncStr, FISH_INC_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, SENSOR_COLOR, SENSOR_STRING_THICKNESS);
	putText(im, fishCountDecstr, FISH_DEC_POINT, FONT_HERSHEY_PLAIN, SENSOR_STRING_SIZE, SENSOR_COLOR, SENSOR_STRING_THICKNESS);

	// ML Data drawn
	for (auto fishML : _objDetectData)
	{
		string scoreStr = "Score: " + to_string(fishML.score);
		putText(im, scoreStr, Point(fishML.ROI.x, fishML.ROI.y - 5), FONT_HERSHEY_PLAIN, 1, (130, 0, 180), 2);

		rectangle(im, fishML.ROI, Scalar(130, 0, 180), 3);
	}

	// Fish ROIs drawn + other information
	for (auto fish : _trackedData)
	{
		rectangle(im, fish.roi, Scalar(0, 255, 0), 3);

		vector<String> rectString;

		rectString.push_back("Area: " + to_string(fish.roi.area()));
		rectString.push_back("Point: <" + to_string(fish.roi.x + fish.roi.width / 2) + ", " + to_string(fish.roi.y + fish.roi.height / 2) + ">");
		Point textPoint = Point(fish.roi.x + 10, fish.roi.y + fish.roi.height + 10);

		for (auto i = 0; i < (int)rectString.size(); i++)
		{
			putText(im, rectString[i], textPoint, FONT_HERSHEY_PLAIN, 1, Scalar(0, 255, 0), 2);
			textPoint.y += 15;
		}
	}

	// Draw line for seeing middle
	int midPoint = _fishTrackerObj.getFrameCenter() * 1/TRACKER_SCALE_FACTOR;
	line(im, Point(midPoint, 0), Point(midPoint, im.size().height), Scalar(0, 255, 255), 2);

	return true;
}

// Sets up PWM by seeing what amoutn of lightis in the cmaera and calibrates
void FishCenS::_setLED()
{
	Mat lightFrame;

	if (_fcfuncs::millis() - _timers["ledTimer"] <= LIGHT_REFRESH)
	{
		return;
	}

	gpioHardwarePWM(LED_PIN, _ledPwmFreq, 100000);
	/*
	if (_ledOff)
	{
		gpioHardwarePWM(LED_PIN, _ledPwmFreq, 0);
		return;
	}

	_timers["ledTimer"] = _fcfuncs::millis();

	// Store the class's camera object into lightFrame
	{
		scoped_lock guard(_frameLock);

		gpioHardwarePWM(LED_PIN, _ledPwmFreq, 0);

		double thisTimer = _fcfuncs::millis();

		while (_fcfuncs::millis() - thisTimer <= 300)
		{
		}

		if (!_cam.getVideoFrame(lightFrame, 1000))
		{
			_fcfuncs::writeLog(_fcLogger, "Timeout error while initializing", true);
		}

		thisTimer = _fcfuncs::millis();

		while (_fcfuncs::millis() - thisTimer <= 200)
		{
		}
	}

	// Lock frame to check lighing
	{
		if (!lightFrame.empty())
		{
			cv::cvtColor(lightFrame, lightFrame, cv::COLOR_RGB2GRAY);
			// Stores the mean of the channels, since all gray only index 0 has value
			Scalar gray_sc = mean(lightFrame);

			// Calculate the amount of light in the frame
			int lightAmt = gray_sc[0];

			lightAmt = sqrt(lightAmt) * 16;

			// std::cout << "Light amount is " << lightAmt << "\n";

			// Map 255-lightAmt to LED_DEFAULT_PWM_MIN - LED_DEFAULT_PWM_MAX
			_ledPwmDC = (255 - lightAmt) * (LED_DEFAULT_PWM_MAX - LED_DEFAULT_PWM_MIN) / 255 + LED_DEFAULT_PWM_MIN;

			// Testing
			std::cout << "Success. LED PWM Value is " << _ledPwmDC << "\n";
		}
		else
		{
			_fcfuncs::writeLog(_fcLogger, "No data in frame");
			std::cout << "Not success. LED PWM Value is " << _ledPwmDC << "\n";
		}

		// Set the PWM value of the gpioPin to pwmVal
		gpioHardwarePWM(LED_PIN, _ledPwmFreq, _ledPwmDC);
	}
	*/
}

void FishCenS::_manageVideoRecord()
{
	if (_mode == fcMode::VIDEO_RECORDER && (_returnKey == 's' || _returnKey == 'S'))
	{
		_recordOn = false;
		_returnKey = '\0';
		std::cout << "Record thread ending ... \n";
	}

	if (_mode == fcMode::VIDEO_RECORDER && (_returnKey == 'r' or _returnKey == 'R'))
	{
		_recordOn = true;
		_returnKey = '\0';

		std::cout << "Record thread starting ... \n";

		thread recordThread(&FishCenS::_videoRunThread, this);
		recordThread.detach();
	}
}

void FishCenS::closePeripherals()
{
	//_depthObj -> setProgramOpen(false);
}

void FishCenS::_trackerUpdate()
{
	std::unique_lock<std::mutex> singleLock(_singletonTracker, std::try_to_lock);

	if (!singleLock.owns_lock())
	{
		return;
	}

	// Copy parameters to local variables to avoid problems in concurrency
	Mat localFrame;
	{
		scoped_lock frameLock(_frameLock);
		localFrame = _frame.clone();
		resize(localFrame, localFrame, Size(_frameSize.width * TRACKER_SCALE_FACTOR, _frameSize.height * TRACKER_SCALE_FACTOR));
	}

	vector<TrackedObjectData> localTrackedData;
	int localFishInc, localFishDec;
	{
		scoped_lock trackerLock(_trackerLock);

		//Reset
		if (_returnKey == '0')
		{
			_returnKey = '\0';
			_fishIncremented=0;
			_fishDecremented=0;
		}

		localFishInc = _fishIncremented;
		localFishDec = _fishDecremented;
	}

	// Update the fish tracker
	int trackerSuccess;
	vector<_ft::fishCountedStruct> fishCounted;
	trackerSuccess = _fishTrackerObj.update(localFrame, localFishInc, localFishDec, localTrackedData, fishCounted);

	for(auto &fish : localTrackedData)
	{
		_fcfuncs::convertRect(fish.roi, (1/TRACKER_SCALE_FACTOR));
	}

	// Store back into class variables
	if (trackerSuccess >= 0)
	{

		scoped_lock trackerLock(_trackerLock);
		_trackedData = localTrackedData;
		_fishIncremented = localFishInc;
		_fishDecremented = localFishDec;
	}

	// Take picture and update SQL table if there are new fish
	if (fishCounted.size() > 0)
	{
		// Check to make sure we still have the correct date
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

		// Get the path to the folder where the pictures will be saved
		string picFolderPath = _picFolderPath;

		// Remove the './' from the path, if it exists, so it can be concatenated later
		if (picFolderPath[0] == '.')
		{
			picFolderPath.erase(0, 2);
		}

		// Get absolute path
		string absPath = fs::absolute(picFolderPath).string();

		// Get current time for recording picture
		string picTime = _fcfuncs::getTimeStamp();
		string filename = absPath + picTime + ".jpg";

		// Save the frame as a picture in the current date folder
		imwrite(filename, localFrame);

		string sqlDate, sqlTime;

		// Get current date and time for sql database
		_fcfuncs::parseDateTime(picTime, sqlDate, sqlTime);

		for (auto fish : fishCounted)
		{
			_fcDatabase::fishCounterData counterData;
			counterData.date = sqlDate;
			counterData.time = sqlTime;
			counterData.direction = fish.dir;
			counterData.filename = filename;
			counterData.roi = fish.roi;

			scoped_lock sqlLock(_sqlLock);
			_sqlObj.insertFishCounterData(counterData);
		}
	}

	vector<FishMLData> localObjDetectData;
	{
		scoped_lock objDetectLock(_objDetectLock);
		localObjDetectData = _objDetectData;
		_objDetectData.clear();
	}

	for(auto &fish : localObjDetectData)
	{
		_fcfuncs::convertRect(fish.ROI, TRACKER_SCALE_FACTOR);
	}

	// Run fish tracker
	if (localObjDetectData.size()>0)
		_fishTrackerObj.generate(localFrame, localObjDetectData);
}

void FishCenS::_trackerUpdateThread(FishCenS *ptr)
{
	ptr->_trackerUpdate();
}

void FishCenS::_MLUpdate()
{
	std::unique_lock<std::mutex> singleLock(_singletonML, std::try_to_lock);

	if (!singleLock.owns_lock())
	{
		return;
	}

	// Copy parameters to local variables to avoid problems in concurrency
	Mat localFrame;
	{
		scoped_lock frameLock(_frameLock);
		localFrame = _frame.clone();
	}

	vector<FishMLData> localObjDetectData;
	{
		scoped_lock objDetectLock(_objDetectLock);
		localObjDetectData = _objDetectData;
	}

	// Run fishML
	if (_fishMLWrapper.update(localFrame, localObjDetectData) < 0)
	{
		return;
	}

	// Store data back into class variable
	{
		scoped_lock objDetectLock(_objDetectLock);
		_objDetectData = localObjDetectData;
	}
}

void FishCenS::_MLUpdateThread(FishCenS *ptr)
{
	ptr->_MLUpdate();
}

void FishCenS::_loadFrame()
{
	//	Next two if statements just load _frame with the proper content,
	//	depending on whether the camera is being read or a test video is
	//	instead being read.

	Mat localFrame;

	// Read camera
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION || _mode == fcMode::VIDEO_RECORDER)
	{
		// Load video frame, if timeout, restart while loop
		if (!_cam.getVideoFrame(localFrame, 1000))
		{
			_fcfuncs::writeLog(_fcLogger, "Timeout error\n", true);
			return;
		}
	}

	// Read video
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		// Load frame and advance position of video
		_vid >> localFrame;
		_vidNextFramePos++;

		// Loop video - Reset video to frame 0 if end of video is reached
		if (_vidNextFramePos >= _vidFramesTotal)
		{
			_vidNextFramePos = 0;
			_vid.set(CAP_PROP_POS_FRAMES, 0);
		}
	}

	// Resize frame
	if (_mode != fcMode::VIDEO_RECORDER)
	{
		//resize(localFrame, localFrame, Size(_videoWidth, _videoHeight));
	}

	// std::unique_lock<std::mutex> frameLock(_frameLock, std::try_to_lock);
	// if (frameLock.owns_lock())
	{
		scoped_lock frameLock(_frameLock);
		_frame = localFrame;
	}
	// else
	// {
	// 	_fcfuncs::writeLog(_fcLogger, "Frame skip\n", true);
	// }

	// Update the pipeline
	if(!_pipelineOff && ((_fcfuncs::millis()-_timers["pipeline"])>=PIPELINE_PERIOD))
	{
		_timers["pipeline"] = _fcfuncs::millis();
		_fishPipe.write(localFrame);
		//cout << "Pipeline time: " << _fcfuncs::millis() - _timers["pipeline"] << "ms" << endl;
	}
}

void FishCenS::_loadFrameThread(FishCenS *ptr)
{
	ptr->_loadFrame();
}