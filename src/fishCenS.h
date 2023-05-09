#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>

#include <map>
#include <thread>

#include "libcamera/lccv.hpp"
#include "tracking/fishTracker.h"
#include "tracking/fishMLWrapper.h"
#include "tracking/fishPipe.h"
#include "misc/videoRecord.h"
#include "sensor/Depth.h"
#include "sensor/Temperature.h"
#include "gui/gui.h"
#include "gui/imex.h"
#include "database/sqlManager.h"

using namespace std;
using namespace cv;
using namespace lccv;

namespace _fc
{
	//Camera width and height
	const int VIDEO_WIDTH	= 640; //px
	const int VIDEO_HEIGHT	= 480; //px
	const double VIDEO_SCALE_FACTOR = 1;
	const double TRACKER_SCALE_FACTOR = 0.5;
	
	const double LIGHT_REFRESH = 1000 * 60 * 30; //ms (1000ms * 60s * 30min)
	
	//Gpio pins
	const int LED_PIN	= 13; //Main LED (big flashy flash)
	
	const string LOGGER_PATH = "./logData"; // Folder path for Logging data
	
	const string TEST_VIDEO_PATH = "./testVideos/"; // Folder for seeing videos - NOTE: CANNOT HAVE SLASH AT END
	const string TEST_VIDEO_FILE = "blue_fish_daytime_1.avi";
	
	const int MAX_LOG_SIZE	= 5000; //Max amount of lines the logger can have before erasing start
	
	//For timers (typically in milliseconds)
	const double DRAW_FPS = 15; //FPS of opencv imshow
	const double DRAW_PERIOD = 1000 / DRAW_FPS; //Period of opencv imshow in milliseconds
	
	const double DEPTH_PERIOD = 2000; //2000 //ms
	const double TEMPERATURE_PERIOD = 2000; //ms
	const double SENSORS_PERIOD = 1000 * 60; //ms -> 1 min
	const double PIPELINE_PERIOD = 100; //ms
	
	const double TRACKING_TIMER = 30; // Just for update loop
	
	const double CAM_FPS = 30;
	
	const int SLEEP_TIMER	= 300; //milliseconds

	//For video listing
	const int VIDEOS_PER_PAGE = 10; //20 listings per page when showing which videos
	
	//For calibration mode
	const double MAX_WIDTH_PER_FRAME = 640;
	const double MAX_HEIGHT_PER_FRAME = 360;
	
	//For putting sensor information
	const Point DEPTH_STRING_POINT = Point(50, VIDEO_HEIGHT - 40);
	const Point TEMP_STRING_POINT = Point(50, VIDEO_HEIGHT - 20);
	
	const Point FISH_INC_POINT = Point(50, 30);
	const Point FISH_DEC_POINT = Point(50, 50);
	
	const double SENSOR_STRING_SIZE = 1;
	const double SENSOR_STRING_THICKNESS = 2;
	
	//LED PWM DC related
	const int LED_DEFAULT_PWM_MIN = 200000;
	const int LED_DEFAULT_PWM_MAX = 500000;
	const int LED_DEFAULT_PWM_INT = 5000;
	const int DEFAULT_LED_FREQ = 500;

	const Scalar YELLOW = Scalar(0, 255, 255);
	const Scalar GREEN = Scalar(0, 255, 0);
	const Scalar BLUE = Scalar(255, 0, 0);
	const Scalar RED = Scalar(0, 0, 255);
	const Scalar TEAL = Scalar(255, 255, 0);
	const Scalar MAGENTA = Scalar(255, 0, 255);
	const Scalar WHITE = Scalar(255, 255, 255);
	const Scalar GREY = Scalar(180, 180, 180);

	const string VIDEO_PATH = "./testVideos/";
	const string PIC_BASE_PATH = "./fishPictures/";

}

enum class fcMode
{        
	TRACKING,
	CALIBRATION,
	TRACKING_WITH_VIDEO,
	CALIBRATION_WITH_VIDEO,
	VIDEO_RECORDER
};

/**
 * @brief Main fishCenS runtime class
 * @details Class that governs and house-keeps fishCenS
 * @author Fish-CenS - Jimmy Bates
 * @date February 18 2023
 *
 **/
class FishCenS
{
public:
	/**
	 *	@brief FishCenS Constructor - Does nothing rn
	 **/
	FishCenS();
	
	/**
	 *	@brief FishCenS Destructor - Does nothing rn
	 **/
	~FishCenS();
	
	/**
	 *	@brief Initiates all variables
	 *	@return 1 if good, -1 if error
	 **/
	int init(fcMode mode);
	
	/**
	 *	@brief Manages runtime of fishCenS object
	 *	@return 1 if good, -1 if error
	 **/	
	int run();
	
	/**
	 *	@brief Return value of waitKey, used for exiting obj
	 *	@return value of _returnKey
	 **/		
	char getReturnKey();

	/**
	 ** @brief Turns testing mode on or off to show important parameters on screen
	 ** @param isTesting true for testing, false to turn testing off
	 ***/
	void setTesting(bool isTesting);

	/**
	 * @brief Turns display on from command line
	*/
	void displayOn() { _displayOn = true; }

	/**
	 * @brief Turns sensors off from command line
	*/
	void sensorsOff() { _sensorsOff = true; }

	/**
	 * @brief Turns LED off from command line
	*/
	void ledOff() { _ledOff = true; }

	/**
	 * @brief Tells sensor objects to close
	*/
	void closePeripherals();

private:
	////////////// PARAMTERS/OBJS //////////////
	
	//For testing mode
	bool _testing;
	bool _displayOn;
	bool _sensorsOff;
	bool _ledOff;

	//Pigpio related stuff
	int _ledPwmFreq;
	int _ledPwmDC;
	int _ledPwmMin;
	int _ledPwmMax;
	int _ledPwmInt;

	//Mode of fishCenS defined by initiation
	fcMode _mode;
	
	//This is attached to cv::WaitKey...
	char _returnKey;	
	
	//Machine learning related things
	FishMLWrapper _fishMLWrapper;
	vector<FishMLData> _objDetectData;
	bool _MLReady;

	//Tracking params
	FishTracker _fishTrackerObj; // Overall tracker
	vector<TrackedObjectData> _trackedData; //Tracked objects
	int _fishIncremented, _fishIncrementedPrev;
	int _fishDecremented, _fishDecrementedPrev; 
	double _scaleFactor; //For calibration mode only

	//Imaging (Camera and mats)
	PiCamera _cam;
	Mat _frame, _frameDraw;
	vector<_ft::returnMatsStruct> _returnMats; //Mats that are processed from the tracking and inrange algs
	bool _ledState;
	Size _frameSize;
	double _camFPS;
	double _camPeriod;
	
	//Video recording
	VideoRecord _vidRecord;
	vrMode _videoRecordState;
	int _videoWidth;
	int _videoHeight;
	bool _recordOn;
	
	//Testing video playback
	VideoCapture _vid;
	int _vidFramesTotal; // For looping video over at last frame
	int _vidNextFramePos;
	double _vidFPS;
	double _vidPeriod;
	
	//Mutex for threadlocking/threading
	vector<thread> _threadVector;
	mutex _baseLock;
	mutex _depthLock;
	mutex _tempLock;
	mutex _videoLock;
	mutex _pwmLock;
	mutex _frameLock; 
	mutex _frameDrawLock;
	mutex _sensorsLock;
	mutex _sqlLock;
	mutex _trackerLock;
	mutex _objDetectLock;
	
	mutex _singletonDraw, _singletonTracker, _singletonUpdate, _singletonML;
	
	//Logger stuff
	vector<string> _fcLogger;
	string _logFileName;
	
	//For data collection
	string _picFolderPath;
	string _currentDate;
	
	//Timers
	map<string, double> _timers;
	
	//Sensors stuff
	//std::unique_ptr<Depth> _depthObj;
	int _depthOpen;
	int _currentDepth;
	double _currentTemp;
	
	//Trackbars and GUI stuff
  	gui _gui_object;

	//Sql database
	sqlManager _sqlObj;

	//Pipeline
	FishPipeline _fishPipe;
	
	////////////// METHODS //////////////
	// Shows video frame (might need to be threaded)
	// returns 1 if good, -1 if error
	int _draw();	
	static void _drawThreadStart(FishCenS* ptr);
	
	void _videoRun();
	static void _videoRunThread(FishCenS* ptr);

	// Runs every minute, and does data collection
	void _sensors();
	static void _sensorsThread(FishCenS* ptr);

	void _trackerUpdate();
	static void _trackerUpdateThread(FishCenS* ptr);

	void _MLUpdate();
	static void _MLUpdateThread(FishCenS* ptr);
	
	void _loadFrame();
	static void _loadFrameThread(FishCenS* ptr);
	
	//Sensors and lights related
	void _setLED();
	void _manageVideoRecord(); 
	
	//Helps list files for video playback initializer
	int _getVideoEntry(string& selectionStr);
	void _showVideoList(vector<string> videoFileNames, int page);
		
	//General helper functions
	int _showRectInfo(Mat& im); // Show rects for ROI, and shows info about them	

};