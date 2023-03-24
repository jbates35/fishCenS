#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>

#include <map>
#include <thread>

#include "libcamera/lccv.hpp"
#include "fishTracker.h"
#include "videoRecord.h"
#include "sensor/Depth.h"
#include "sensor/Temperature.h"
#include "gui/gui.h"
#include "gui/imex.h"

using namespace std;
using namespace cv;
using namespace lccv;

namespace _fc
{
	//Camera width and height
	const int VIDEO_WIDTH	= 768/2; //px
	const int VIDEO_HEIGHT	= 432/2; //px
	
	const double LIGHT_REFRESH = 1000 * 60 * 30; //ms (1000ms * 60s * 30min)
	
	//Gpio pins
	const int LED_PIN	= 13; //Main LED (big flashy flash)
	
	const string LOGGER_PATH = "./logData"; // Folder path for Logging data
	
	const string TEST_VIDEO_PATH = "./testVideos/"; // Folder for seeing videos - NOTE: CANNOT HAVE SLASH AT END
	const string TEST_VIDEO_FILE = "blue_fish_daytime_1.avi";
	
	const int MAX_LOG_SIZE	= 5000; //Max amount of lines the logger can have before erasing start
	
	//For timers (typically in milliseconds)
	const double DRAW_FPS = 30; //FPS of opencv imshow
	const double DRAW_PERIOD = 1000 / DRAW_FPS; //Period of opencv imshow in milliseconds
	
	const double DEPTH_PERIOD = 2000; //2000 //ms
	const double TEMPERATURE_PERIOD = 2000; //ms
	
	const int SLEEP_TIMER	= 300; //milliseconds

	//For video listing
	const int VIDEOS_PER_PAGE = 10; //20 listings per page when showing which videos
	
	//For calibration mode
	const double MAX_WIDTH_PER_FRAME = 640;
	const double MAX_HEIGHT_PER_FRAME = 360;
	
	//For putting sensor information
	const Point DEPTH_STRING_POINT = Point(50, VIDEO_HEIGHT - 40);
	const Point TEMP_STRING_POINT = Point(50, VIDEO_HEIGHT - 20);
	
	const Point FISH_COUNT_POINT = Point(50, 30);
	const Point FISH_TRACKED_POINT = Point(50, 50);
	
	const double SENSOR_STRING_SIZE = 1;
	const double SENSOR_STRING_THICKNESS = 2;
	
	//LED PWM DC related
	const int LED_DEFAULT_PWM_MIN = 100000;
	const int LED_DEFAULT_PWM_MAX = 1000000;
	const int LED_DEFAULT_PWM_INT = 100000;
	const int DEFAULT_LED_FREQ = 1000000;

	const Scalar YELLOW = Scalar(0, 255, 255);
	const Scalar GREEN = Scalar(0, 255, 0);
	const Scalar BLUE = Scalar(255, 0, 0);
	const Scalar RED = Scalar(0, 0, 255);
	const Scalar TEAL = Scalar(255, 255, 0);
	const Scalar MAGENTA = Scalar(255, 0, 255);
	const Scalar WHITE = Scalar(255, 255, 255);
	const Scalar GREY = Scalar(180, 180, 180);
}

enum class fcMode
{        
	TRACKING,
	CALIBRATION,
	TRACKING_WITH_VIDEO,
	CALIBRATION_WITH_VIDEO,
	VIDEO_RECORDER
};

using namespace _fc;

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

private:
	////////////// PARAMTERS/OBJS //////////////
	
	//For testing mode
	bool _testing;

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
	
	//Tracking params
	FishTracker _fishTrackerObj; // Overall tracker
	vector<Rect> _ROIRects; //Needs opencv4.5+ - rects that show ROIs of tracked objects for putting on screen
	int _fishCount; 
	double _scaleFactor; //For calibration mode only

	//Imaging (Camera and mats)
	PiCamera _cam;
	Mat _frame, _frameDraw;
	vector<_ft::returnMatsStruct> _returnMats; //Mats that are processed from the tracking and inrange algs
	bool _ledState;
	
	//Video recording
	VideoRecord _vidRecord;
	vrMode _videoRecordState;
	int _videoWidth;
	int _videoHeight;
	
	//Testing video playback
	VideoCapture _vid;
	int _vidFramesTotal; // For looping video over at last frame
	int _vidNextFramePos;
	double _vidFPS;
	double _vidPeriod;
	
	//Mutex for threadlocking/threading
	vector<thread> _threadVector;
	mutex _baseLock, _depthLock, _tempLock;
	mutex _pwmLock;
	mutex _frameLock; 
	
	//Logger stuff
	vector<string> _fcLogger;
	string _logFileName;
	
	//Timers
	map<string, double> _timers;
	
	//Sensors stuff
	Depth _depthObj;
	int _depthSerialOpen;
	int _currentDepth;
	double _currentTemp;
	
	//Trackbars and GUI stuff
  	gui _gui_object;
	
	////////////// METHODS //////////////
	
	// Sets up video frames for showing. Anything tracking is done here
	//	returns 1 if good, -1 if error	
	int _update();
	static void _updateThreadStart(FishCenS* ptr);
	
	
	// Shows video frame (might need to be threaded)
	// returns 1 if good, -1 if error
	int _draw();	
	static void _drawThreadStart(FishCenS* ptr);
	
	void _videoRun();
	static void _videoRunThread(FishCenS* ptr);

	void _trackingUpdate(); //takes care of normal tracking, and tracking with video (alpha mode)
	void _calibrateUpdate();
	
	//Sensors and lights related
	void _setLED();

	//ALPHA MODES
	void _videoRecordUpdate();
	
	//Helps list files for video playback initializer
	int _getVideoEntry(string& selectionStr);
	void _showVideoList(vector<string> videoFileNames, int page);
		
	//General helper functions
	static string _getTime(); // Returns time (this is used in a few diff classes, maybe it should be deferred to a separate helper file?)
	static double _millis(); //Returns ms to be used with getTickCount/getTickFreq
	void _log(string data, bool outputToScreen = false); //Writes to log vector, outputs to screen if need be
	int _saveLogFile(); //Ensures data folder exists and saves log file there 
	int _showRectInfo(Mat& im); // Show rects for ROI, and shows info about them	

};