#include "fishTracker.h"
#include "misc/fcFuncs.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

//Nothing is needed in constructor, using init() instead
FishTracker::FishTracker()
{
}

FishTracker::~FishTracker()
{
	cout << "\tNOTE: FishTracker destroyed\n";
}


bool FishTracker::run(Mat& im, vector<returnMatsStruct>& returnMats, mutex& lock, int& fishCount, vector<Rect>& ROIRects)
{		
	//Make new Mat so it can be used without needing mutex
	Mat frameRaw, frameNoBG, frameProcessed, frameMask, frameGrayscale;
	frameRaw = Mat::zeros(_frameSize, CV_8UC3);
	
	{
		//Make automatic mutex control
		scoped_lock guard(lock);

		//Make sure there's actually a frame to do something with
		if (im.empty())
		{
			_logger(_loggerData, "Could not load frame, exiting thread");
			return false;
		}
	
		//Clear mats that will be returned
		returnMats.clear();
		
		frameRaw = im.clone();
	}
	//First, get mask of image...
	_pBackSub->apply(frameRaw, frameMask);

	//Copy only parts of the image that moved
	frameRaw.copyTo(frameNoBG, frameMask);
	
	//cvtColor(frameRaw, frameGrayscale, COLOR_BGR2GRAY);

	//Get time for measuring elapsed tracking time
	_timer = _fcfuncs::millis();
		
	//Update tracker, and ensure it's running
	//THIS FOLLOWING CODE MAY BE HARD TO READ AND MIGHT NEED TO BE CLEANED UP SOMEHOW
	for (int i = _fishTracker.size() - 1; i >= 0; i--) 
	{
		//Update ROI from tracking			********** CHANGE FOLLOWING LINE FOR TRACKER IF NEED BE **************
		_fishTracker[i].isTracked = _fishTracker[i].tracker->update(frameRaw, _fishTracker[i].roi);
		
		//Make sure we aren't detecting the same object twice
		for (int j = _fishTracker.size() - 1; j > i; j--)
		{
			//If roi overlap, same object
			if ((_fishTracker[j].roi & _fishTracker[i].roi).area() > _minCombinedRectArea)
			{
				_fishTracker.erase(_fishTracker.begin() + j);
			}
		}
		
		//Move onto next iteration if cannot be detected anymore
		if (!_fishTracker[i].isTracked)
		{
			continue;	
		}			
			
		//Following script sees if the ball has passed through the midpoint, and from which side
		//If it passes from left to right, we increase the ball counter. If the other way, we decrease
		//Additionally, some code has been put in to make sure we aren't counting vector additions from multiple runs
		_fishTracker[i].posX.push_back(_fishTracker[i].roi.x + _fishTracker[i].roi.width / 2);				
					
		if (_fishTracker[i].posX.size() >= 2)
		{
			int posCurr = _fishTracker[i].posX[1];
			int posLast = _fishTracker[i].posX[0];
						
			if (posCurr > _frameMiddle && posLast <= _frameMiddle)
			{
				_logger(_loggerData, "Ball count ++");
				fishCount++;	
				_countedROI = _fishTracker[i].roi;

			}
			else if (posCurr <= _frameMiddle && posLast > _frameMiddle)
			{
				_logger(_loggerData, "Ball count --");
				fishCount--;	
				_countedROI = _fishTracker[i].roi;					
			}	
			
			//Delete unnecessary position values
			_fishTracker[i].posX.erase(_fishTracker[i].posX.begin());
		}										
	}
	
	//log info for timer if needed
	if (_fishTracker.size() != 0)
	{
		_logger(_loggerData, "Time ellapsed for all tracker updates: " + to_string(_fcfuncs::millis() - _timer) + "ms");
		_loggerCsv.push_back(_fcfuncs::millis() - _timer);
	}
	
	//Start process time for thresholding
	_timer = _fcfuncs::millis();
	
	//Change Color to HSV for easier thresholding
	cvtColor(frameNoBG, frameProcessed, COLOR_BGR2HSV);
	

	//Threshold the image for finding contours
	inRange(frameProcessed, Scalar(10, 0, 0), Scalar(180, 240, 240), frameProcessed);
	
	//	//Erode and dilate elements for eroding and dilating image
	Mat erodeElement = getStructuringElement(MORPH_ELLIPSE, _erodeSize);
	Mat dilateElement = getStructuringElement(MORPH_ELLIPSE, _dilateSize);
	
	//Now erode and dilate image so that contours may be picked up a bit more (really, washes away unused thresholded parts)
	for (int i = 0; i < _erodeAmount; i++)
	{
		erode(frameProcessed, frameProcessed, erodeElement);
	}
	
	for (int i = 0; i < _dilateAmount; i++)
	{	
		dilate(frameProcessed, frameProcessed, dilateElement);			
	}
	
	//Image is ready to find contours, so first create variables we can find contours in
	vector<vector<Point>> contours;
	
	//Find contours so we can find center and other variables needed for tracking
	findContours(frameProcessed, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);		
	
	//Go through these contours and find the parameters of each
	for (int i = 0; i < (int) contours.size(); i++)
	{
		//Get area of contour
		double thisContourArea = contourArea(contours[i]);
		
		//Find roi from contours
		Rect contourRect = boundingRect(contours[i]);
		
		//If in calibration mode, show that a rect is found here for information
		if (_programMode == ftMode::CALIBRATION)
		{
			Scalar thisColor = Scalar(255, 0, 255);
			Point thisPoint = Point(contourRect.x + contourRect.width / 2, contourRect.y - contourRect.height / 2 + 5);
			putText(frameProcessed, "Area: " + to_string(thisContourArea), thisPoint, FONT_HERSHEY_PLAIN, 0.4, thisColor);
			rectangle(frameProcessed, contourRect, thisColor, 2);					
		}
			
		//Only process it if meets over certain threshold
		if (thisContourArea >= _minThresholdArea && thisContourArea <= _maxThresholdArea)
		{			
			//Test-tell me what area that is
			_logger(_loggerData, "Area of found ball is... " + to_string(thisContourArea)); 

			//Make roi smaller that will actually be tracked
			//First need center
			Point centerPoint = Point(contourRect.x + contourRect.width / 2, contourRect.y + contourRect.height / 2);
				
			//Make scaled width
			int newWidth = (contourRect.width / 2) * _rectROIScale;
			newWidth = (newWidth == 0) ? 1 : newWidth;
				
			//Make scaled width
			int newHeight = (contourRect.height / 2) * _rectROIScale;
			newHeight = (newHeight == 0) ? 1 : newHeight;
							
			//Make rect that will be used for tracking
			Rect contourRectROI = Rect(centerPoint.x - newWidth, centerPoint.y - newHeight, newWidth * 2, newHeight * 2);
							
			_logger(_loggerData, "Time ellapsed for thresholding to find first ball is " + to_string(_fcfuncs::millis() - _timer) + "ms");
					
			//See if this contour rectangle is an object that's already been detected
			bool fishOverlappedROIs = false; // Turns true if the contour rect overlaps with one of the fishTracker rois
			for (auto& fish : _fishTracker)
			{					
				//Checks for overlap and stores result
				fishOverlappedROIs |= ((contourRect & fish.roi).area() > _minCombinedRectArea);
	
				//Can exit loop if there's been an overlap
				if (fishOverlappedROIs)
				{
					break;
				}					
			}
			
			//Now see if a tracker has been lost, and if a widened area (by a reasonable amount) overlaps with the contour
			//If so, then have that specific tracker "re-track" with the contour
			bool fishRetracked = false;
			for (auto& fish : _fishTracker)
			{
				
				//Skip iteration if tracker hasn't been lost
				if (fish.isTracked)
				{ 
					continue;
				}
					
				//Make wider ROI to account for moving ball
				Rect newFishROI = Rect(fish.roi.x - _retrackPixels, fish.roi.y - _retrackPixels, fish.roi.width + _retrackPixels * 2, fish.roi.height + _retrackPixels * 2);
						
				if ((contourRect & newFishROI).area() > _minCombinedRectArea)
				{
					fishRetracked = true;	
					fish.isTracked = true;
						
					//Re-initialize the tracker with the new coordinates
					TrackerKCF::Params params;
					fish.tracker = TrackerKCF::create(params);
					fish.tracker->init(frameRaw, contourRectROI);
					
					_logger(_loggerData, "Object retracked");
				}
			}
				
			//One last thing that needs to be checked is if this is a reflection
			//The method I can think of best doing this is by creating an artificial rect for both the input and output, and comparing the two
			bool isReflection = false;
			Rect currentContourRect = Rect(contourRect.x, 0, contourRect.x + contourRect.width, 2);
			
			for (auto& fish : _fishTracker)
			{
				Rect newFishROIRect = Rect(fish.roi.x - EXTRA_REFLECT_WIDTH, 0, fish.roi.x + fish.roi.width + EXTRA_REFLECT_WIDTH, 3);
				
				//Two things we need to check:
				// 1.) Is this contour ABOVE the ROI of a tracked object?
				// 2.) Is this contour's x horizontally between the start and end of the tracked object?
				if (contourRect.y < fish.roi.y && (newFishROIRect & currentContourRect).area() > 0)
				{
					isReflection = true;
					break;				
				}			
			}
			
			//Now need to iterate through the other contours
			if (!isReflection)
			{				
				for (int j = 0; j < (int) contours.size(); j++) 
				{
					//Disregard this iteration of loop if it's the same contour
					if (i == j)
					{
						continue;						
					}
					
					Rect comparedContour = boundingRect(contours[j]);
					Rect comparedContourNewRect = Rect(comparedContour.x - EXTRA_REFLECT_WIDTH, 0, comparedContour.x + comparedContour.width + EXTRA_REFLECT_WIDTH, 2);
				
					if (contourRect.y < comparedContour.y && (comparedContourNewRect & currentContourRect).area() > 0)
					{
						isReflection = true;
						break;				
					}		
				}
			}
			
			//If no overlap, then make new struct and track
			if (!fishOverlappedROIs && !fishRetracked && !isReflection)
			{									
				//Initialize struct that keeps track of the tracking info
				FishTrackerStruct tempTracker;					
				tempTracker.isTracked = true;
				tempTracker.tracker = TrackerKCF::create();					
				tempTracker.tracker->init(frameRaw, contourRectROI);
				tempTracker.roi = contourRectROI;
				tempTracker.posX.push_back(contourRectROI.x + contourRectROI.width/2);
				tempTracker.lostFrameCount = 0;
				tempTracker.startTime = _fcfuncs::millis();
				tempTracker.currentTime = _fcfuncs::millis() - tempTracker.startTime;
					
				//Now add that to the overall fish tracker
				_fishTracker.push_back(tempTracker);						
				
				_logger(_loggerData, "Object found");
			}
			
			//Add information about contour to the processed frame
			if (_programMode == ftMode::CALIBRATION)
			{
				Scalar infoColor = Scalar(255, 255, 255);
				Point infoPoint = Point(contourRectROI.x + 5, contourRectROI.y - 10);
				
				String infoString = "OBJECT FOUND";
				if (isReflection) infoString = "REFLECTION";
				if (fishRetracked) infoString = "RETRACKABLE";
				if (fishOverlappedROIs) infoString = "ALREADY TRACKED";
				
				putText(frameProcessed, infoString, infoPoint, FONT_HERSHEY_PLAIN, 0.6, infoColor);
				rectangle(frameProcessed, contourRectROI, infoColor, 3);
			}
						
			/********** DELME LATER *************/
			if (_testing)
			{
				if (isReflection)
				{
					Scalar thisColour = Scalar(0, 0, 255);
					rectangle(frameRaw, contourRect, thisColour, 3);
					putText(frameRaw, "Reflection", Point(contourRect.x, contourRect.y - 6), FONT_HERSHEY_PLAIN, 0.7, thisColour, 2);
				}
				else 
				{
					Scalar thisColour = Scalar(0, 255, 0);
					rectangle(frameRaw, contourRect, thisColour, 3);
					putText(frameRaw, "No reflection", Point(contourRect.x, contourRect.y - 6), FONT_HERSHEY_PLAIN, 0.7, thisColour, 2);	
				}
			}
			
		}
	}		
		
	//Parse through all fish trackers, from end to beginning, and do housekeeping
	//Meaning: 1. Time gets updated, and delete if timeout (ONLY if ROI isn't in center though)
	//2. delete tracked object if tracking has been lost
	//3. need to check if the contour is a reflection. Need to delete if it is a reflection
	
	//First, get edges
	vector<Rect> edges;
	
	edges.push_back(Rect(0, 0, _frameSize.width*_marginProportion, _frameSize.height)); // left
	edges.push_back(Rect(_frameSize.width*(1 - _marginProportion), 0, _frameSize.width*_marginProportion, _frameSize.height)); // right
	edges.push_back(Rect(0, 0, _frameSize.width, _frameSize.height *_marginProportion * 0.5)); // top
	edges.push_back(Rect(0, _frameSize.height * (1 - _marginProportion * 0.5), _frameSize.width, _frameSize.height *_marginProportion * 0.5)); // bottom
	
	if (_testing)
	{
		for (auto edge : edges) 
		{
			rectangle(frameRaw, edge, Scalar(150, 150, 150), 2);
		}
	}
	
	//Now parse through fishTrackers
	//Must go backwards else this algorithm will actually skip elements	
	for (int i = _fishTracker.size() - 1; i >= 0; i--) 
	{	
		
		_fishTracker[i].currentTime = _fcfuncs::millis() - _fishTracker[i].startTime;
		bool trackedObjectOutdated = _fishTracker[i].currentTime > TRACKER_TIMEOUT_MILLIS;
		
		//if (_testing)
		{
			putText(frameRaw, "Time ellapsed: " + to_string((int) _fishTracker[i].currentTime) + "ms", _fishTracker[i].roi.tl() + Point(40, -10), FONT_HERSHEY_PLAIN, 0.6, Scalar(190, 190, 190));
		}
		
		//Delete if timeout and isn't in center of screen
		if (trackedObjectOutdated)// && !(_frameMiddle > _fishTracker[i].roi.x && _frameMiddle < _fishTracker[i].roi.x + _fishTracker[i].roi.width))
		{
			_fishTracker.erase(_fishTracker.begin() + i);
			_logger(_loggerData, "TRACKING TIMEOUT FOR ELEMENT: " + to_string(i + 1));	
			continue; // Move onto next iteration
		}
		
		if (!_fishTracker[i].isTracked)
		{	
			//Count amount of frames the fish has been lost for
			_fishTracker[i].lostFrameCount++;
				
			//See if the item is just on the edge
			bool isOnEdge = false;
			
			for (auto edge : edges) 
			{
				isOnEdge |= (edge & _fishTracker[i].roi).area() > 0;
			}		
			
						
			if (_fishTracker[i].lostFrameCount >= _retrackFrames || isOnEdge)
			{
				//After trying to retrack a while, element cannot be found so it should be removed from overall vector list
				_fishTracker.erase(_fishTracker.begin() + i);
				_logger(_loggerData, "TRACKING LOST FOR ELEMENT: " + to_string(i + 1));	
				continue;
			}
		}
		else
		{
			//Reset if fish has been found again
			_fishTracker[i].lostFrameCount = 0;
		}
		
		//Check to see if there's another object below it, and if so delete it from tracked vector list
		Rect modifiedCurrentRect = Rect(_fishTracker[i].roi.x, 0, _fishTracker[i].roi.x + _fishTracker[i].roi.width, 3);
			
		for (int j = 0; j < (int) _fishTracker.size(); j++) 
		{
				
			//Don't bother if it is the same number
			if (i == j)
			{
				continue;
			}
				
			Rect modifiedCompareRect = Rect(_fishTracker[j].roi.x, 0, _fishTracker[j].roi.x + _fishTracker[j].roi.width, 3);
				
			//IF this is true, that means tracked object is directly above another tracked object
			//This could be improved, but for now we will consider that a reflection
			//(How could it be improved? Two objects could be real above and beneath each other)
			if (_fishTracker[i].roi.y < _fishTracker[j].roi.y && (modifiedCompareRect & modifiedCurrentRect).area() > 0)
			{
				_fishTracker.erase(_fishTracker.begin() + i);
				_logger(_loggerData, "REFLECTION DETECTED, DELETING OBJECT: " + to_string(i + 1));		
				break;					
			}
		}
	}
	
	//Return first mat no matter what 
	// ->	If more efficiency is needed, the following scope can be removed
	//		and then fishCenS.cpp will need to be adjusted so 
	//		that during the hconcat/vconcat, frame -> frameDraw
	{		
		//Make automatic mutex control
		lock_guard<mutex> guard(lock);
		
		//Make struct for storing in returnMats
		returnMatsStruct tempMatStruct;
		
		//Mask mat
		tempMatStruct.title = "NORMAL";
		tempMatStruct.colorMode = imgMode::BGR;
		frameRaw.copyTo(tempMatStruct.mat);
		returnMats.push_back(tempMatStruct);
	
	}	
	
	//Returns Mats if need be
	if (_programMode == ftMode::CALIBRATION)
	{	
		//Make automatic mutex control
		lock_guard<mutex> guard(lock);
		
		//Make struct for storing in returnMats
		returnMatsStruct tempMatStruct;

		//Mask mat
		tempMatStruct.title = "MASK";
		tempMatStruct.colorMode = imgMode::BINARY;
		frameMask.copyTo(tempMatStruct.mat);
		returnMats.push_back(tempMatStruct);
		
		//BG-Removed mat
		tempMatStruct.title = "BG REMOVED";
		tempMatStruct.colorMode = imgMode::BGR;
		frameNoBG.copyTo(tempMatStruct.mat);
		returnMats.push_back(tempMatStruct);		
		
		//Processed binary image
		tempMatStruct.title = "PROCESSED";
		tempMatStruct.colorMode = imgMode::BINARY;
		frameProcessed.copyTo(tempMatStruct.mat);
		returnMats.push_back(tempMatStruct);		
	}	
	
	
	//Copy rects from fishTracker vector to parent class
	ROIRects.clear();
	ROIRects = _getRects();
	
	return true;
}


bool FishTracker::init(unsigned int video_width, unsigned int video_height, Scalar rangeMin /* = Scalar(0,0,0) */, Scalar rangeMax /* = Scalar(180,255,255) */)
{
	//Clear fish tracking vector
	_fishTracker.clear();
	
	//Emptying data vectors	
	_loggerData.clear();
	_loggerCsv.clear();
	
	//Test mode for seeing important parameters
	_testing = false;
	
	//Background removal object initialization
	_pBackSub = cv::createBackgroundSubtractorKNN();

	//Default params for erode
	_erodeSize = DEFAULT_ERODE_SIZE;
	_erodeAmount = DEFAULT_ERODE_AMOUNT;

	//Default params for dilate	
	_dilateSize = DEFAULT_DILATE_SIZE;
	_dilateAmount = DEFAULT_DILATE_AMOUNT;
	
	//Store temporary frame to get Size and middle
	_frameSize = Size(video_width, video_height);
	_frameMiddle = _frameSize.width / 2;

	//Ranges for HSV thresholding
	_rangeMin = rangeMin;
	_rangeMax = rangeMax;
	
	//Thresholding minimum area of contour to throw into tracker
	_minThresholdArea = DEFAULT_MIN_AREA;
	_maxThresholdArea = DEFAULT_MAX_AREA;
	
	//Minimum area that combined ROIs can be before they are considered a "tracked object" already
	_minCombinedRectArea = DEFAULT_COMBINED_RECT_AREA;
	
	//A percentage specified of the frame to be considered the edge
	//If the frame were 1000 pixels, and 0.08 is specified, then 80 pixels from the left
	//And 80 pixels from the right will be considered the edge
	_marginProportion = DEFAULT_MARGIN;
	
	//When an object is lost, there is occlusion detection. This specifies the area around
	//the latest ROI entry as to where to look for an untracked object
	_retrackPixels = DEFAULT_RETRACK_PIXELS;
	
	//When object is not on the edge, amount of frames to try to "re-track" a lost object
	_retrackFrames = DEFAULT_RETRACK_FRAMES;
	
	//Proportion of ROI to scale down by to increase computational speed of tracker
	_rectROIScale = DEFAULT_RECT_SCALE;
		
	//Defaults for logger path and filename
	_startTime = _fcfuncs::getTimeStamp();
	_loggerFilename = "data_" + _startTime;
	_loggerFilepath = DEFAULT_FILE_PATH;
	
	//Test mode to display possible helpful debugging parameters
	_programMode = ftMode::TRACKING;
	
	return true;
}

//Setter for erode size
void FishTracker::setErode(Size erodeSize)
{
	_erodeSize = erodeSize;
}

//Getter for erode size
Size FishTracker::getErode()
{
	return _erodeSize;
}
	
//Setter for dilate size
void FishTracker::setDilate(Size dilateSize)
{
	_dilateSize = dilateSize;
}

//Getter for dilate size
Size FishTracker::getDilate()
{
	return _dilateSize;
}

int FishTracker::getErodeAmount()
{
	return _erodeAmount;
}


void FishTracker::setErodeAmount(int thisErodeAmount)
{
	_erodeAmount = thisErodeAmount;
}


int FishTracker::getDilateAmount()
{
	return _dilateAmount;
}


void FishTracker::setDilateAmount(int thisDilateAmount)
{
	_dilateAmount = thisDilateAmount;
}

//Setting HSV range for when you want to change both
void FishTracker::setRange(Scalar rangeMin, Scalar rangeMax)
{
	_rangeMin = rangeMin;
	_rangeMax = rangeMax;
}

//Setting HSV range for just either min or max
void FishTracker::setRange(Scalar range, int type)
{
	if (type == RANGE_MAX)
	{
		_rangeMax = range;
	}
	if (type == RANGE_MIN)
	{
		_rangeMin = range;
	}
}

//Get HSV range
Scalar FishTracker::getRange(int type)
{
	//Either min or max, depending on type
	Scalar returnScalar;
	
	if (type == RANGE_MIN)
	{
		returnScalar = _rangeMin;
	}
	else if (type == RANGE_MAX)
	{
		returnScalar = _rangeMax;
	}
	
	return returnScalar;
}

//Set proportion of frame that will be considered the "edge"
void FishTracker::setMargin(float marginProportion)
{
	_marginProportion = marginProportion;
}

//Returns value of frame margin proportion
float FishTracker::getMargin()
{
	return _marginProportion;
}

//Set region that occlusion detector will look around lost object
void FishTracker::setRetrackRegion(int retrackPixels)
{
	_retrackPixels = retrackPixels;
}

//Returns value for occlusion detector region
int FishTracker::getRetrackRegion()
{
	return _retrackPixels;
}

//Sets amount of frames occlusion detector will run before deleting tracker vector
void FishTracker::setRetrackFrames(int retrackFrames)
{
	_retrackFrames = retrackFrames;
}

//Returns amount of frames occlusion detector will run before deleting tracker vector
int FishTracker::getRetrackFrames()
{
	return _retrackFrames;
}

//Sets a scalar value that multiplies tracker ROI to save computation time
void FishTracker::setROIScale(float rectROIScale)
{
	_rectROIScale = rectROIScale;
}

//Returns scalar value for multiplying tracker ROI 
float FishTracker::getROIScale()
{
	return _rectROIScale;
}

//Sets minimum threshold that a contour must be over to be added into a tracker
void FishTracker::setMinThresholdArea(int minArea)
{
	_minThresholdArea = minArea;
}

//Returns minimum threshold that a contour must be over to be added into a tracker
int FishTracker::getMinThresholdArea()
{
	return _minThresholdArea;
}

//Sets area two ROIs overlapping is before it's considered the ROI of a tracked object
void FishTracker::setMinCombinedRectArea(int minArea)
{
	_minCombinedRectArea = minArea;
}

//Gets area two ROIs overlapping is before it's considered the ROI of a tracked object
int FishTracker::getMinCombinedRectArea()
{
	return _minCombinedRectArea;
}

//Sets the x_position the vertical line will be which fish crossing over will be counted
void FishTracker::setFrameCenter(int center)
{
	_frameMiddle = center;
}

//Gets the x_position the vertical line will be which fish crossing over will be counted
int FishTracker::getFrameCenter()
{
	return _frameMiddle;
}


//Sets the program mode, i.e. calibration, normal, testing, etc.
void FishTracker::setMode(ftMode mode)
{
	_programMode = mode;
}

//Describes whether the tracker is object or not
bool FishTracker::isTracking()
{
	return _fishTracker.size() > 0;
}

//Returns amount of fishTracker objects being tracked
int FishTracker::trackerAmount()
{
	return _fishTracker.size();
}

//Setter for filepath
void FishTracker::setFilepath(string filepath)
{
	_loggerFilepath = filepath;
}

//Setter for filename
void FishTracker::setFilename(string filename)
{
	_loggerFilename = filename;
}

//Helper function for 
void FishTracker::_logger(vector<string>& logger, string data)
{
	//Clear first entry in the vector if buffer size is full
	if (logger.size() > MAX_DATA_SIZE)
	{
		logger.erase(logger.begin());
	}
	
	string logString = _fcfuncs::getTimeStamp() + ": " + data;
	//cout << logString << "\n";
	logger.push_back(logString);
}

//Saves both data and elapsed tracking frame times to files
void FishTracker::saveLogger(string fileName /* = NULL */, string filePath /* = NULL */)
{
	//Use file-name stored in object if filename is blank
	if (fileName.empty())
	{
		fileName = _loggerFilename;		
	}
	
	//Use folder-path stored in object if filePath is blank
	if (filePath.empty())
	{
		filePath = _loggerFilepath;
	}
	
	//Add slash if folder-path does not end in slash so filename doesn't merge with the folder it's in
	if (filePath[filePath.size() - 1] != '/')
	{
		filePath += '/';
	}
	
	//Make sure file doesn't end in ".txt" if we end up being knobs and accidentally adding it
	stringstream tempSString(fileName);
	string tempString;
	fileName = "";
	
	while (getline(tempSString, tempString, '.'))
	{
		if (tempString != "csv" && tempString != "txt")
		{
			fileName += tempString;
		}
	}
	
	//Create txt file for data and then store the data
	ofstream outFileData(filePath + fileName + ".txt");
	for (auto line : _loggerData)
	{
		outFileData << line;
	}
	outFileData.close();
	
	//Create CSV file and store data
	ofstream outFileCSV(filePath + fileName + ".csv");
	for (auto val : _loggerCsv) 
	{
		outFileCSV << to_string(val) << "\n";
	}
	outFileCSV.close();
}

//Helper function to pull ROIs from fishTracker vector and return them as its own vector
vector<Rect> FishTracker::_getRects()
{
	//Since the rects are embedded in structs, need to extract them into separate vector
	vector<Rect> tempRects;
	
	for (auto fish : _fishTracker)
	{
		tempRects.push_back(fish.roi);
	}
	
	return tempRects;
}


void FishTracker::setTesting(bool isTesting)
{
	_testing = isTesting;
}
