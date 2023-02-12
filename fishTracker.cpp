#include "fishTracker.h"

//Nothing is needed in constructor, using init() instead
FishTracker::FishTracker()
{
}

FishTracker::~FishTracker()
{
}


bool FishTracker::run(Mat& im, Mat& imProcessed, mutex& lock, int& fishCount, vector<Rect>& ROIRects)
{
	//Make automatic mutex control
	lock_guard<mutex> guard(lock);

	//Make sure there's actually a frame to do something with
	if (im.empty())
	{
		_logger(_loggerData, "Could not load frame, exiting thread");
		return false;
	}
	
	//Make new Mat so it can be used without needing mutex
	Mat frameRaw, frameProcessed, frameMask;
	im.copyTo(frameRaw);
	im.copyTo(frameProcessed);
	
	//Get time for measuring elapsed tracking time
	_timer = _millis();

	//Update tracker, and ensure it's running
	//THIS FOLLOWING CODE MAY BE HARD TO READ AND MIGHT NEED TO BE CLEANED UP SOMEHOW
	for (int i = _fishTracker.size() - 1; i >= 0; i--) 
	{
		//Update ROI from tracking
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
			}
			else if (posCurr <= _frameMiddle && posLast > _frameMiddle)
			{
				_logger(_loggerData, "Ball count --");
				fishCount--;						
			}	
			
			//Delete unnecessary position values
			_fishTracker[i].posX.erase(_fishTracker[i].posX.begin());
		}										
	}
	
	//log info for timer if needed
	if (_fishTracker.size() != 0)
	{
		_logger(_loggerData, "Time ellapsed for all tracker updates: " + to_string(_millis() - _timer) + "ms");
		_loggerCsv.push_back(_millis() - _timer);
	}
	
	//Start process time for thresholding
	_timer = _millis();
	
	//Change Color to HSV for easier thresholding
	cvtColor(frameProcessed, frameProcessed, COLOR_BGR2HSV);
	
	//Erode and dilate elements for eroding and dilating image
	Mat erodeElement = getStructuringElement(MORPH_RECT, _erodeSize);
	Mat dilateElement = getStructuringElement(MORPH_RECT, _dilateSize);
	
	//Now erode and dilate image so that contours may be picked up a bit more (really, washes away unused thresholded parts)
	for (int i = 0; i < _erodeAmount; i++)
	{
		erode(frameProcessed, frameProcessed, erodeElement);
	}
	
	for (int i = 0; i < _dilateAmount; i++)
	{	
		dilate(frameProcessed, frameProcessed, dilateElement);			
	}
	
	//Threshold the image for finding contours
	inRange(frameProcessed, _rangeMin, _rangeMax, frameProcessed);
			
	//Image is ready to find contours, so first create variables we can find contours in
	vector<vector<Point>> contours;
	
	//Find contours so we can find center and other variables needed for tracking
	findContours(frameProcessed, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);	
	
	//Go through these contours and find the parameters of each
	for (int i = 0; i < contours.size(); i++)
	{
		//Get area of contour
		double thisContourArea = contourArea(contours[i]);
			
		//Only process it if meets over certain threshold
		if (thisContourArea >= _minThresholdArea)
		{			
			//Test-tell me what area that is
			_logger(_loggerData, "Area of found ball is... " + to_string(thisContourArea)); 
					
			//Find roi from contours
			Rect contourRect = boundingRect(contours[i]);
				
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
							
			_logger(_loggerData, "Time ellapsed for thresholding to find first ball is " + to_string(_millis() - _timer) + "ms");
			
			double timeStart = _millis();
					
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
					fish.tracker = TrackerKCF::create();
					fish.tracker->init(frameRaw, contourRectROI);
						
					_logger(_loggerData, "Object retracked");
				}
			}
				
			//If no overlap, then make new struct and track
			if (!fishOverlappedROIs && !fishRetracked)
			{					
				//Initialize struct that keeps track of the tracking info
				FishTrackerStruct tempTracker;					
				tempTracker.isTracked = true;
				tempTracker.tracker = TrackerKCF::create();					
				tempTracker.tracker->init(frameRaw, contourRectROI);
				tempTracker.roi = contourRectROI;
				tempTracker.posX.push_back(contourRectROI.x);
				tempTracker.lostFrameCount = 0;
					
				//Now add that to the overall fish tracker
				_fishTracker.push_back(tempTracker);		
				
				_logger(_loggerData, "Object found");
			}																	
		}		
			
		//Parse through all fish trackers, from end to beginning, and execute any code if tracking has been lost
		//Must go backwards else this algorithm will actually skip elements
		for (int i = _fishTracker.size() - 1; i >= 0; i--) 
		{
			if (!_fishTracker[i].isTracked)
			{	
				//Count amount of frames the fish has been lost for
				_fishTracker[i].lostFrameCount++;
					
				//See if the item is just on the edge
				bool isOnEdge = false;
					
				//Sees if the object is on the horizontal edges of the frame
				if ((_fishTracker[i].roi.x + _fishTracker[i].roi.width / 2) < _frameSize.width*_marginProportion || (_fishTracker[i].roi.x + _fishTracker[i].roi.width / 2) > _frameSize.width*(1 - _marginProportion))
				{
					isOnEdge = true;
				}
					
				if (_fishTracker[i].lostFrameCount >= _retrackFrames || isOnEdge == true)
				{
					//After trying to retrack a while, element cannot be found so it should be removed from overall vector list
					_fishTracker.erase(_fishTracker.begin() + i);
					_logger(_loggerData, "TRACKING LOST FOR ELEMENT: " + to_string(i + 1));											
				}
			}
			else
			{
				//Reset if fish has been found again
				_fishTracker[i].lostFrameCount = 0;
			}				
		}
		

		//Delete this if we're not interested in having the processed frame in the main script
		frameProcessed.copyTo(imProcessed);
		
		//Copy rects from fishTracker vector to parent class
		ROIRects.clear();
		ROIRects = _getRects();
		
	}
	
	return true;
}


bool FishTracker::init(unsigned int video_width, unsigned int video_height, Scalar rangeMin /* = Scalar(0,0,0) */, Scalar rangeMax /* = Scalar(180,255,255) */)
{
	//Clear fish tracking vector
	_fishTracker.clear();
	
	//Emptying data vectors	
	_loggerData.clear();
	_loggerCsv.clear();
	
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
	_startTime = _getTime();
	_loggerFilename = "data_" + _startTime;
	_loggerFilepath = DEFAULT_FILE_PATH;
	
	//Test mode to display possible helpful debugging parameters
	_testMode = OFF;
	
	return true;
}

//Sets testMode for displaying important parameters 
void FishTracker::setTestMode(int testMode)
{
	_testMode = testMode;
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

//Helper that gets the current time in ymd hms and returns a string
std::string FishTracker::_getTime()
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


//Return current time in millis like arduino
double FishTracker::_millis()
{
	return 1000 * getTickCount() / getTickFrequency();
}

//Helper function for 
void FishTracker::_logger(vector<string>& logger, string data)
{
	//Clear first entry in the vector if buffer size is full
	if (logger.size() > MAX_DATA_SIZE)
	{
		logger.erase(logger.begin());
	}
	
	logger.push_back(_getTime() + ": " + data + "\n");
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