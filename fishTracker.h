#pragma once

#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>

#include <mutex>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <thread>

using namespace cv;
using namespace std;

enum
{
	RANGE_MIN,
	RANGE_MAX
};

enum
{
	OFF,
	ON		
};


//Default params for eroding the image to separate balls
#define DEFAULT_ERODE_SIZE Size(2,2)
#define DEFAULT_ERODE_AMOUNT 3

//Default params for dilating the image to smooth edges
#define DEFAULT_DILATE_SIZE Size(4,4)
#define DEFAULT_DILATE_AMOUNT 1

//Max size of the vector that holds all the data, to prevent wildly large files
#define MAX_DATA_SIZE 5000

//Minimum area to initiate tracker to
#define DEFAULT_MIN_AREA 2000

//Minimum area of combined rect before it's considered "overlapping" 
#define DEFAULT_COMBINED_RECT_AREA 0

//Default region of ROI 
#define DEFAULT_RECT_SCALE 1

//Proportional margin of the camera frame that will be 
#define DEFAULT_MARGIN 0.1

//When an object is lost, the amount of pixels surrounding the last ROI it will look for an untracked object
#define DEFAULT_RETRACK_PIXELS 100

//Amount of frames occlusion will try to re-track an object for, before deleting from the tracking vector
#define DEFAULT_RETRACK_FRAMES 5

//File path logger gets saved to by default
#define DEFAULT_FILE_PATH "./"

//Maximum amount of time object can be tracked before it's dropped from the tracker
#define MAX_TRACKER_DURATION 30 //(seconds)

//Struct keeping track of parameters of fish
struct FishTrackerStruct
{
	Ptr<Tracker> tracker;
	vector<int> posX;
	bool isTracked;
	Rect roi;
	int lostFrameCount;
    double startTime;
};

/**
 * @brief Fish Tracking class for Fish-CenS
 * @details Class that  you can pass a CV MAT and then allows you to track objects using KCF.
 * @author Fish-CenS - Jimmy Bates
 * @date January 29 2023
 *
 **/
class FishTracker
{
public:
    /**
     * @brief Constructor, does nothing, as init() is used
     **/
	FishTracker();

    /**
     * @brief Destructor does nothing currently
     **/
	~FishTracker();
	
    /**
     * @brief Run thread for parent class
     * @param im - Main image from VideoCapture (do video >> frame and pass that)
     * @param imProcessed Image that's processed, mainly for calibrating/testing (think of this as a return)
     * @param lock Mutex passed from parent class
     * @param fishCount Amount of fish counted via 
     * @param ROIRects Vector of ROI rects parent class can have updated
     * @return True if success, false if im is empty 
     **/
	bool run(Mat& im, Mat& imProcessed, mutex& lock, int& fishCount, vector<Rect>& ROIRects);
	
    /**
     * @brief Initializer (And reinitializer) for most important parameters of class
     * @param video_width Width of video
     * @param video_height Height of video
     * @param rangeMin optional scalar for min HSV Values to be threshold'd
     * @param rangeMax optional scalar for max HSV values to be threshold'd
     * @return True if success, false if no frame was passed or empty frame was passed
     **/
	bool init(unsigned int video_width, unsigned int video_height, Scalar rangeMin = Scalar(0, 0, 0), Scalar rangeMax = Scalar(180, 255, 255));
	
    /**
     * @brief Test mode boolean to display certain parameters on screen
     * @param testMode OFF (0) for no test mode, ON (1) for test mode
     **/
	void setTestMode(int testMode);	
	
    /**
    * @brief Setter for erode size
    * @param erodeSize cv::Size of erode 
    **/
	void setErode(Size erodeSize);

    /**
    * @brief Getter for erode size
    * @return Erode size (default Size(1,1))
    **/
	Size getErode();
	
    /**
    * @brief Setter for dilate size
    * @param dilateSize cv::Size of dilate (default Size(4,4))
    **/
	void setDilate(Size dilateSize);	
	
    /**
    * @brief Getter for dilate size
    * @return Dilate size
    **/
	Size getDilate();
	
	/**
    * @brief Sets the range of HSV values that the image will be thresholded with
    * @param rangeMin Scalar of minimum range of HSV
    * @param rangeMax Scalar of maximum range of HSV
    **/
	void setRange(Scalar rangeMin, Scalar rangeMax);
	
    /**
    * @brief Sets range of thresholding HSV values for minimum or maximum
    * @param range Range that will be set
    * @param type Either MINIMUM (0) or MAXIMUM (1)
    **/
	void setRange(Scalar range, int type);	
	
    /**
    * @brief Getter for HSV range that is used for thresholding image
    * @param type Whether MINIMUM (0) or MAXIMUM (1) HSV value gets returned
    * @return Scalar range of specified HSV value
    **/
	Scalar getRange(int type);
	
	/**
    * @brief Setter for margin of the camera shot that occlusion detection works on
    * @param marginProportion Proportion of frame that gets considered edge (default 0.1)
    **/
	void setMargin(float marginProportion);
	
    /**
    * @brief Getter of margin of the camera shot that occlusion detection works on
    * @return Proportion of frame that is considered edge
    **/
	float getMargin();
	
	/**
    * @brief Setter for retrack region - When an object is lost in the middle of the frame, the tracker will attempt to find an untracked object that is THIS amount of pixels around the latest ROI
    * @param retrackPixels Region around the latest tracker's ROI it will attempt to look for an untracked object (default is 100)
    **/
	void setRetrackRegion(int retrackPixels);
	
    /**
    * @brief Getter for retrack region - When an object is lost in the middle of the frame, the tracker will attempt to find an untracked object that is THIS amount of pixels around the latest ROI
    * @return Region around the latest tracker's ROI it will attempt to look for an untracked object
    **/
	int getRetrackRegion();
	
	/**
    * @brief Setter for retrackFrames
    * @param retrackFrames Once an object is lost, the amount of frames the tracker will look for the object before deleting the tracker from the tracker vector (default is 5)
    **/
	void setRetrackFrames(int retrackFrames);
	
    /**
    * @brief Getter for retrackFrames
    * @return Once an object is lost, the amount of frames the tracker will look for the object before deleting the tracker from the tracker vector (default is 5)
    **/
	int getRetrackFrames();	
	
	/**
    * @brief Setter for rectROIScale
    * @param rectROIScale Once the contours of an image is found, the ROI can be scaled by this amount before being fed into the Tracker
    **/
	void setROIScale(float rectROIScale);	
	
    /**
    * @brief Getter for rectROIScale
    * @return Once the contours of an image is found, the ROI can be scaled by this amount before being fed into the Tracker
    **/
	float getROIScale();
	
	/**
    * @brief Setter for threshhold minimum area
    * @param minArea Minimum area that a thresholded contour has to be in order to be thrown into the tracker algorithm
    **/
	void setMinThresholdArea(int minArea);
	
    /**
    * @brief Getter for threshhold minimum area
    * @return Minimum area that a thresholded contour has to be in order to be thrown into the tracker algorithm
    **/
	int getMinThresholdArea();
	
	/**
    * @brief Setter for minimum combined rect area
    * @param minArea Minimum area two ROIs must be before it's considered "overlapped"
    **/
	void setMinCombinedRectArea(int minArea);
	
    /**
    * @brief Getter for minimum combined rect area
    * @return Minimum area two ROIs must be before it's considered "overlapped"
    **/
	int getMinCombinedRectArea();
	
	/**
    * @brief Setter for frameCenter (overrides the initial value)
    * @param center The center of frame that is used to see if the fish have crossed, increments/decrements fishcounter accordingly
    **/
	void setFrameCenter(int center);
	
    /**
    * @brief Getter for frameCenter
    * @param center The center of frame that is used to see if the fish have crossed, increments/decrements fishcounter accordingly
    **/
	int getFrameCenter();
	
	/**
    * @brief Setter for filepath
    * @param filepath Path that the logger and other files that are saved will get saved to
    **/
	void setFilepath(string filepath);	
	
	/**
    * @brief Setter for filename
    * @param filename Name of prefix parts of the filename
    **/
	void setFilename(string filename);
	
	/**
    * @brief At that moment, saves all the information in the log stringstreams to a file
    * @param fileName If something is put in, will save to this file name
    * @param filePath If something is put in, will save to this file path
    **/
	void saveLogger(string fileName = NULL, string filePath = NULL);
	
private:
	////////// PARAMETERS ///////////
	
	//Parameters for thresholding
	Size _erodeSize;
	int _erodeAmount;
	Size _dilateSize;
	int _dilateAmount;
	Scalar _rangeMin;
	Scalar _rangeMax;
	int _minThresholdArea; // Contour area must be greater than this to be added to tracker
	int _minCombinedRectArea; // Minimum shared area between overlapping rects
	
    //Parameters for BG Removal
    Ptr<BackgroundSubtractor> _pBackSub;

	//Parameters for tracking
	vector<FishTrackerStruct> _fishTracker; //Holds all tracked object information
	float _marginProportion; //Essentially, percentage margins should be set to to consider object "on the edge"
	int _retrackPixels; //When occlusion occurs, what size of area around the ROI to look for an untracked object
	int _retrackFrames; //When an object has been lost, how many frames to keep looking for the object before deleting object from vector
	float _rectROIScale; //What percentage to delete from the roi when tracking

	//Parameters for counting
	Size _frameSize; // Takes the size and calculates mid
	int _frameMiddle; // Middle of screen for counting fish
	
	//Parameters for logging
	double _timer; //For putting elapsed times in
	string _startTime;
	string _loggerFilepath;
	string _loggerFilename;
	vector<string> _loggerData; // Keeps track of important events that happen
	vector<double> _loggerCsv; // Writes elapsed tracking computation times 
	
	//Other important data
	int _testMode;
	
	/////////// FUNCTIONS /////////////
	
	//Get date and time in yyyy_mm_dd_hxxmxxsxx
	string _getTime();
	double _millis();
	void _logger(vector<string>& logger, string data);
	vector<Rect> _getRects();
	
	
};