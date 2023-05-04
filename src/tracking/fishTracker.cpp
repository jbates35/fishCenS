#include "fishTracker.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>

#include <opencv2/tracking/tracking_legacy.hpp>

using namespace FishMLHelper;

//Nothing is needed in constructor, using init() instead
FishTracker::FishTracker()
{
}

FishTracker::~FishTracker()
{
	cout << "\tNOTE: FishTracker destroyed\n";
}

int FishTracker::update(Mat &im, int &fishIncrement, int &fishDecrement, vector<TrackedObjectData> &trackedObjects)
{
	//First, get edges for comparison at multiple times through loop
	vector<Rect> edges;
	edges.push_back(Rect(0, 0, _frameSize.width * _marginProportion, _frameSize.height)); // left edge
	edges.push_back(Rect(_frameSize.width * (1 - _marginProportion), 0, _frameSize.width, _frameSize.height)); // right edge
	edges.push_back(Rect(0, _frameSize.height - 2, _frameSize.width, 2)); // bottom edge
	edges.push_back(Rect(0, 0, _frameSize.width, 2)); // top edge
	
	//These are to get rid of the ROIs that seem to pop up out of nowhere in the center
	vector<Rect> vertEdges;
	vertEdges.push_back(Rect(Point((_frameSize.width * 0.5) - 10,0), Point((_frameSize.width * 0.5) + 10, _frameSize.height * TOP_AND_BOTTOM_CLIPS)));
	vertEdges.push_back(Rect(Point((_frameSize.width * 0.5) - 10, _frameSize.height * (1 - TOP_AND_BOTTOM_CLIPS)), Point((_frameSize.width * 0.5) + 10, _frameSize.height)));

	//Make sure there's actually a frame to do something with
	if (im.empty())
	{
		cout << "ERROR: No frame to process in FishTracker::update()\n";
		return -1;
	}

	if(im.size() != _frameSize)
	{
		cout << "ERROR: Frame size does not match in FishTracker::update()\n";
		return -1;
	}

	//Update tracker, and ensure it's running
	//THIS FOLLOWING CODE MAY BE HARD TO READ AND MIGHT NEED TO BE CLEANED UP SOMEHOW
	for (int i = _fishTracker.size() - 1; i >= 0; i--) 
	{
		//Update ROI from tracking			********** CHANGE FOLLOWING LINE FOR TRACKER IF NEED BE **************
		{
			scoped_lock trackerLock(_trackerLock);
			_fishTracker[i]->isTracked = _fishTracker[i]->tracker->update(im, _fishTracker[i]->roi);
		}

		//Make sure we aren't detecting the same object twice
		for (int j = _fishTracker.size() - 1; j > i; j--)
		{
			bool isOverlapped = false;

			Rect rect1 = _fishTracker[i]->roi;
			Rect rect2 = _fishTracker[j]->roi;
			
			int area1 = rect1.area();
			int area2 = rect2.area();

			// Check if one is inside the other
			if(_fishTracker[i]->isCounted || _fishTracker[j]->isCounted)
			{
				isOverlapped |= (rect1 & rect2).area() >= area1 * 0.3;
				isOverlapped |= (rect1 & rect2).area() >= area2 * 0.3;
			}
			else
			{
				isOverlapped |= (rect1 & rect2).area() >= area1 * 0.1;
				isOverlapped |= (rect1 & rect2).area() >= area2 * 0.1;
			}
			//If roi overlap, same object
			if (isOverlapped)
			{
				_fishTracker.erase(_fishTracker.begin() + j);
			}
		}
		
		//Move onto next iteration if cannot be detected anymore
		if (!_fishTracker[i]->isTracked)
		{
			continue;	
		}									
	}
		
	//Parse through all fish trackers, from end to beginning, and do housekeeping
	//Meaning: 1. Time gets updated, and delete if timeout (ONLY if ROI isn't in center though)
	//2. delete tracked object if tracking has been lost
	//3. need to check if the contour is a reflection. Need to delete if it is a reflection
	
	//Now parse through fishTrackers
	//Must go backwards else this algorithm will actually skip elements	
	for (int i = _fishTracker.size() - 1; i >= 0; i--) 
	{	
		_fishTracker[i]->currentTime = millis() - _fishTracker[i]->startTime;

		bool trackedObjectOutdated = _fishTracker[i]->currentTime > TRACKER_TIMEOUT_MILLIS;
		bool centeredObjectOutdated = _fishTracker[i]->currentTime > TRACKER_TIMEOUT_MILLIS_CENTER;
		bool isInMiddle = (_frameMiddle > _fishTracker[i]->roi.x && _frameMiddle < _fishTracker[i]->roi.x + _fishTracker[i]->roi.width);

		//Delete if timeout and not in center
		if ((trackedObjectOutdated && !isInMiddle) || (centeredObjectOutdated && isInMiddle))
		{
			_fishTracker.erase(_fishTracker.begin() + i);
			continue; // Move onto next iteration
		}

		bool trackObjectOudatedCenter = _fishTracker[i]->currentTime > TRACKER_TIMEOUT_MILLIS_CENTER;
		
		if (!_fishTracker[i]->isTracked)
		{	
			//Count amount of frames the fish has been lost for
			_fishTracker[i]->lostFrameCount++;
				
			//See if the item is just on the edge
			bool isOnEdge = false;
			
			for (auto edge : edges) 
			{
				isOnEdge |= (edge & _fishTracker[i]->roi).area() > 0;
			}		

			bool isWeirdCenter = (_fishTracker[i]->roi & vertEdges[0]).area() && (_fishTracker[i]->roi & vertEdges[1]).area();
									
			if (_fishTracker[i]->lostFrameCount >= _retrackFrames || (isOnEdge && _fishTracker[i]->currentTime > 2000) || isWeirdCenter)
			{
				//After trying to retrack a while, element cannot be found so it should be removed from overall vector list
				_fishTracker.erase(_fishTracker.begin() + i);
				continue;
			}
		}
		else
		{
			//Reset if fish has been found again
			_fishTracker[i]->lostFrameCount = 0;
		}
	}
	
	//Track through all fish trackers
	for(int i = _fishTracker.size()-1; i>=0; i--)
	{
		//Following script sees if the fish has passed through the midpoint, and from which side
		//If it passes from left to right, we increase the ball counter. If the other way, we decrease
		//Additionally, some code has been put in to make sure we aren't counting vector additions from multiple runs
		_fishTracker[i]->posX.push_back(_fishTracker[i]->roi.x + _fishTracker[i]->roi.width / 2);				
					
		if (_fishTracker[i]->posX.size() >= 2)
		{
			int posCurr = _fishTracker[i]->posX[1];
			int posLast = _fishTracker[i]->posX[0];

			//If the fish is in the middle, we don't want to count it
			if(_fishTracker[i]->isCounted)
			{
				continue;
			}

			if (posCurr > _frameMiddle && posLast <= _frameMiddle)
			{
				fishIncrement++;	
				_fishTracker[i]->isCounted=true;
				continue;				
			}
			else if (posCurr < _frameMiddle && posLast >= _frameMiddle)
			{
				fishDecrement++;	
				_fishTracker[i]->isCounted=true;
				continue;
			}
			
			//Delete unnecessary position values
			_fishTracker[i]->posX.erase(_fishTracker[i]->posX.begin());
		}			
	}

	//Copy rects from fishTracker vector to parent class
	trackedObjects.clear();

	for (auto &fish : _fishTracker)
	{
		TrackedObjectData tempStruct;

		{
			tempStruct.roi = fish->roi;
			tempStruct.currentTime = fish->currentTime;
		}

		trackedObjects.push_back(tempStruct);
	}

	return 0;
}

int FishTracker::generate(Mat& im, vector<FishMLData>& detectedObjects)
{	
	//First, get edges for comparison at multiple times through loop
	vector<Rect> edges;
	edges.push_back(Rect(0, 0, _frameSize.width * _marginProportion, _frameSize.height)); // left edge
	edges.push_back(Rect(_frameSize.width * (1 - _marginProportion), 0, _frameSize.width, _frameSize.height)); // right edge
	edges.push_back(Rect(0, _frameSize.height - 2, _frameSize.width, 2)); // bottom edge
	edges.push_back(Rect(0, 0, _frameSize.width, 2)); // top edge
	
	//These are to get rid of the ROIs that seem to pop up out of nowhere in the center
	vector<Rect> vertEdges;
	vertEdges.push_back(Rect(Point((_frameSize.width * 0.5) - 10,0), Point((_frameSize.width * 0.5) + 10, _frameSize.height * TOP_AND_BOTTOM_CLIPS)));
	vertEdges.push_back(Rect(Point((_frameSize.width * 0.5) - 10, _frameSize.height * (1 - TOP_AND_BOTTOM_CLIPS)), Point((_frameSize.width * 0.5) + 10, _frameSize.height)));

	//Make sure there's actually a frame to do something with
	if (im.empty())
	{
		cout << "ERROR: No frame to process in FishTracker::run()\n";
		return -1;
	}
				
	//Go through these contours and find the parameters of each
	for (auto &obj : detectedObjects)
	{
		int newWidth, newHeight;
		Point centerPoint;

		//Make roi smaller that will actually be tracked
		//First need center
		centerPoint = Point(obj.ROI.x + obj.ROI.width / 2, obj.ROI.y + obj.ROI.height / 2);

		//Scale machine learning rectangle based on its area for tracker efficiency
		double rectROIScale;
		double objArea = (double) obj.ROI.area();
		rectROIScale = -0.0000353 * objArea + 1;

		//Make sure it's not too big or too small
		if(rectROIScale > 0.9)
			rectROIScale = 0.9;
		if(rectROIScale < 0.3)
			rectROIScale = 0.3;

		//Make scaled width
		newWidth = (obj.ROI.width / 2) * rectROIScale;
		newWidth = (newWidth == 0) ? 1 : newWidth;
				
		//Make scaled width
		newHeight = (obj.ROI.height / 2) * rectROIScale;
		newHeight = (newHeight == 0) ? 1 : newHeight;

	
		//Make rect that will be used for tracking
		Rect contourRectROI = Rect(centerPoint.x - newWidth, centerPoint.y - newHeight, newWidth * 2, newHeight * 2);
					
		//See if this contour rectangle is an object that's already been detected
		bool fishOverlappedROIs = false; // Turns true if the contour rect overlaps with one of the fishTracker rois
		
		//Iterate through _fishTracker vector
		for (auto &fish : _fishTracker)
		{	
			//Make sure not null
			if(!fish.get())
			{
				continue;
			}

			//Checks for overlap and stores result
			//fishOverlappedROIs |= ((obj.ROI & fish.roi).area() > _minCombinedRectArea);

			//Check to see if the roi is within the other, as well
			//fishOverlappedROIs |= (obj.ROI & fish.roi).area() >= min(obj.ROI.area(), fish.roi.area()) * _minCombinedRectAreaProportion;

			//Now check whether the contour rect is within the roi
			if(fish->isCounted)
			{
				fishOverlappedROIs |= (obj.ROI & fish->roi).area() >= obj.ROI.area() * 0.2;
				fishOverlappedROIs |= (obj.ROI & fish->roi).area() >= fish->roi.area() * 0.2;
			}
			else
			{
				fishOverlappedROIs |= (contourRectROI & fish->roi).area() >= contourRectROI.area() * 0.2;
				fishOverlappedROIs |= (contourRectROI & fish->roi).area() >= fish->roi.area() * 0.2;
			}
		}
	
		//Now see if a tracker has been lost, and if a widened area (by a reasonable amount) overlaps with the contour
		//If so, then have that specific tracker "re-track" with the contour
		bool fishRetracked = false;
		for (auto& fish : _fishTracker)
		{
			//Make sure not null
			if (!fish.get())
			{
				continue;
			}

			//Skip iteration if tracker hasn't been lost
			if (fish->isTracked)
			{ 
				continue;
			}
				
			//Make wider ROI to account for moving ball
			Rect newFishROI = Rect(fish->roi.x - _retrackPixels, fish->roi.y - _retrackPixels, fish->roi.width + _retrackPixels * 2, fish->roi.height + _retrackPixels * 2);
			
			if ((obj.ROI & newFishROI).area() > _minCombinedRectArea)
			{
				fishRetracked = true;	
				fish->isTracked = true;
					
				//Re-initialize the tracker with the new coordinates
				{			
					scoped_lock trackerLock(_trackerLock);
					fish->tracker = TrackerKCF::create(_params);
					fish->tracker->init(im, contourRectROI);
				}
				break;
			}
		}
	
		bool isOnEdge = false;
		for (auto edge : edges)
		{
			isOnEdge |= (obj.ROI & edge).area() > obj.ROI.area() * 0.3;
		}

		if(isOnEdge)
		{
			cout << "Fish on edge" << endl;
			continue;
		}

		if((vertEdges[0] & obj.ROI).area() && (vertEdges[1] & obj.ROI).area())
		{
			cout << "Artifact ROI" << endl;
			continue;
		}

		//Make rect mostly in center of screen, from top of screen to bottom
		Rect centerRect = Rect(_frameSize.width * 0.4, 0, _frameSize.width * 0.2, _frameSize.height);
		bool isCenter = (obj.ROI & centerRect).area() > 0;

		//If no overlap, then make new struct and track
		if (!fishOverlappedROIs && !fishRetracked && isCenter)
		{									
			//Initialize struct that keeps track of the tracking info
			auto tempTracker = std::make_unique<FishTrackerStruct>();

			tempTracker->isTracked = true;
			tempTracker->tracker = TrackerKCF::create(_params);					
			tempTracker->tracker->init(im, contourRectROI);
			tempTracker->roi = contourRectROI;
			tempTracker->posX.push_back(contourRectROI.x + contourRectROI.width / 2);
			tempTracker->lostFrameCount = 0;
			tempTracker->startTime = millis();
			tempTracker->currentTime = millis() - tempTracker->startTime;
			tempTracker->isCounted = false;
			
			scoped_lock trackerLock(_trackerLock);

			//Now add that to the overall fish tracker
			_fishTracker.push_back(std::move(tempTracker));	
		}
	}
	
	return 0;
}


int FishTracker::init(Size frameSize)
{
	//Clear fish tracking vector
	_fishTracker.clear();
		
	//Test mode for seeing important parameters
	_testing = false;

	//Area of the ROIs that overlap before it is considered a "tracked object" already
	_minCombinedRectAreaProportion = DEFAULT_COMBINED_RECT_AREA_PROPORTION;
	
	//Store temporary frame to get Size and middle
	_frameSize = frameSize;
	_frameMiddle = _frameSize.width / 2;

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
		
	//Test mode to display possible helpful debugging parameters
	_programMode = ftMode::TRACKING;
	
	//Some tracker stuff
	_params = TrackerKCF::Params();
	_params.sigma = 0.1f;
	_params.detect_thresh = 0.4f;
	_params.resize = true;

	return 0;
}

//Describes whether the tracker is object or not
bool FishTracker::isTracking()
{
	return _fishTracker.size() > 0;
}

//Helper function to pull ROIs from fishTracker vector and return them as its own vector
vector<Rect> FishTracker::_getRects()
{
	//Since the rects are embedded in structs, need to extract them into separate vector
	vector<Rect> tempRects;
	
	for (auto &fish : _fishTracker)
	{
		tempRects.push_back(fish->roi);
	}
	
	return tempRects;
}


void FishTracker::setTesting(bool isTesting)
{
	_testing = isTesting;
}
