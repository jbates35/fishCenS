#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>

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
	const int VIDEO_WIDTH	= 768;
	const int VIDEO_HEIGHT	= 432;
	
	//Gpio pins
	const int LED_PIN	= 21; //Main LED (big flashy flash)
	
	const string LOGGER_PATH = "./logData/"; // Folder path for Logging data
	
	const string TEST_VIDEO_PATH = "./testVideos/"; // Folder for seeing videos
	const string TEST_VIDEO_FILE = "blue_fish_daytime_1.avi";
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

	//Imaging (Camera and mats)
	PiCamera _cam;
	Mat _frame;
	vector<_ft::returnMatsStruct> returnMats; //Mats that are processed from the tracking and inrange algs
	
	//Video recording
	vrMode _videoRecordState;
	int _videoWidth;
	int _videoHeight;
	
	//Testing video playback
	VideoCapture vid;
	
	//Mutex for threadlocking
	mutex _baseLock;
	
	//Logger vector
	vector<string> _fcLogger;
	
	////////////// METHODS //////////////
	void _tracking(); //takes care of normal tracking, and tracking with video (alpha mode)
	void _calibrate();
	
	//ALPHA MODES
	void _videoRecord();
	
	void _getTime();
	double _millis();
	void _log(string data, bool say = true);
	
};