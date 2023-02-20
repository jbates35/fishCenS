#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>

#include <map>
#include <thread>

#include "lccv.hpp"
#include "fishTracker.h"
#include "videoRecord.h"
#include "Depth.h"
#include "Temperature.h"

using namespace std;
using namespace cv;
using namespace lccv;

namespace _fc
{
	//Camera width and height
	const int VIDEO_WIDTH	= 768; //px
	const int VIDEO_HEIGHT	= 432; //px
	
	//Gpio pins
	const int LED_PIN	= 21; //Main LED (big flashy flash)
	
	const string LOGGER_PATH = "./logData"; // Folder path for Logging data
	
	const string TEST_VIDEO_PATH = "./testVideos/"; // Folder for seeing videos - NOTE: CANNOT HAVE SLASH AT END
	const string TEST_VIDEO_FILE = "blue_fish_daytime_1.avi";
	
	const int MAX_LOG_SIZE	= 5000; //Max amount of lines the logger can have before erasing start
	
	//For timers (typically in milliseconds)
	const double DRAW_FPS = 30; //FPS of opencv imshow
	const double DRAW_PERIOD = 1000 / DRAW_FPS; //Period of opencv imshow in milliseconds
	
	const double ULTRASONIC_PERIOD = 2000; //ms
	const double TEMPERATURE_SENSOR = 2000; //ms
	
	const int SLEEP_TIMER	= 300; //milliseconds

	//For video listing
	const int VIDEOS_PER_PAGE = 10; //20 listings per page when showing which videos
	
	//For calibration mode
	const double MAX_WIDTH_PER_FRAME = 640;
	const double MAX_HEIGHT_PER_FRAME = 360;
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
	 *	@brief Sets up video frames for showing. Anything tracking is done here
	 *	@return 1 if good, -1 if error
	 **/	
	int update();

	/**
	 *	@brief Shows video frame (might need to be threaded)
	 *	@return 1 if good, -1 if error
	 **/		
	int draw();
	
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

private:
	////////////// PARAMTERS/OBJS //////////////
	
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
	
	//Video recording
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
	mutex _baseLock;
	
	//Logger stuff
	vector<string> _fcLogger;
	string _logFileName;
	
	//Timers
	map<string, double> _timers;
	
	//Sensors stuff
	
	
	////////////// METHODS //////////////
	void _trackingUpdate(); //takes care of normal tracking, and tracking with video (alpha mode)
	void _calibrateUpdate();
	
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
	
};