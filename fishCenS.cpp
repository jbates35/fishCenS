#include "fishCenS.h"

#include <fstream>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <pigpio.h>

// Add simple GUI elements
#define CVUI_DISABLE_COMPILATION_NOTICES
#define CVUI_IMPLEMENTATION
#include "cvui.h"


FishCenS::FishCenS()
{
}


FishCenS::~FishCenS()
{
	destroyAllWindows();
	_cam.stopVideo();
	gpioWrite(LED_PIN, 0);
}

int FishCenS::init(fcMode mode)
{		
	
	//Mode of fishCenS object
	_mode = mode;
	
	//Return key from waitKey
	_returnKey = '\0';
	
	//Tracking and fish counting
	_fishCount = 0;
	_fishTrackerObj.init(VIDEO_WIDTH, VIDEO_HEIGHT);
	
	//Video stuff that needs to be initialized
	_videoRecordState = vrMode::VIDEO_OFF;
	
	//initiate camera	
	if (_mode == fcMode::TRACKING || _mode == fcMode::CALIBRATION)
	{
		_cam.options->video_width = VIDEO_WIDTH;
		_cam.options->video_height = VIDEO_HEIGHT;
		_cam.options->verbose = 1;
		_cam.options->list_cameras = true;
		_cam.options->framerate = 100;
		_cam.startVideo();
			
		//Load video frame, if timeout, restart while loop
		if (!_cam.getVideoFrame(_frame, 1000))
		{
			cout << "Timeout error, exiting program\n";
			return -1;
		}
			
		//If there's nothing in the video frame, also exit
		if (_frame.empty())
		{
			cout << "Camera doesn't work, frame empty\n";
			return -1;
		}
		
		//Set class video width and height for tracker
		_videoWidth = _frame.size().width;
		_videoHeight = _frame.size().height;
	}
	
	//Initiate video file 
	if (_mode == fcMode::TRACKING_WITH_VIDEO || _mode == fcMode::CALIBRATION_WITH_VIDEO)
	{
		vid.open(TEST_VIDEO_PATH + TEST_VIDEO_FILE, CAP_FFMPEG);
	}
	
	//Initiate tracking
	if (_mode == fcMode::TRACKING || _mode == fcMode::TRACKING_WITH_VIDEO)
	{
		//Initialize tracker now
		_fishTrackerObj.init(VIDEO_WIDTH, VIDEO_HEIGHT);
		_ROIRects.clear();
	}
	
	//Initiate calibration parameters

}


int FishCenS::update()
{
}


int FishCenS::run()
{
}

