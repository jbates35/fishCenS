#include <iostream>

#include <opencv2/opencv.hpp>

#include <fstream>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <memory>

#include "lccv.hpp"
#include "fishTracker.h"

using namespace std;
using namespace cv;
using namespace lccv;

// Add simple GUI elements
#define CVUI_DISABLE_COMPILATION_NOTICES
#define CVUI_IMPLEMENTATION
#include "cvui.h"

#define VIDEO_WIDTH 768/1.8
#define VIDEO_HEIGHT 432/1.8

//CHANGE THESE
#define FOLDER_PATH "/home/dev/testvid/" // Folder path for your Pi
#define LOGGER_PATH "/home/dev/shared/trackingTesterData/" // Folder path for Logging data

//Maxes and mins for thresholding balls in HSV
#define BALL_H_MIN 106
#define BALL_H_MAX 180
#define BALL_S_MIN 157
#define BALL_S_MAX 255
#define BALL_V_MIN 145
#define BALL_V_MAX 255

//Contours min and maxes
#define BALL_MIN_AREA 1500

//Amount of width/height/x/y (i.e. the margin on the roi) that's added to re-track the ball
#define RETRACK_REGION 100 

//Another method - try to scale the ROI, need center point tho 
#define RECT_SCALE 0.8

//Max amount of frames a ball can be lost before discarding vector
#define MAX_LOST_FRAMES 5

//The margin on the frame that the fish will get deleted
#define MARGIN_SCALE 0.12

// Add simple GUI elements
#define CVUI_DISABLE_COMPILATION_NOTICES
#define CVUI_IMPLEMENTATION
#include "cvui.h"

void trackingThreadStart(FishTracker& thisObj, Mat& im, Mat& imProc, mutex& lock, int& fishCount, vector<Rect>& ROIRects);

int main(int argc, char *argv[])
{		
	
	//Parameters for tracking fish, ROIs, and fish counter
	FishTracker fishTrackerObj;
	vector<Rect> ROIRects;	
	int fishCount=0;

	//Mutex for running tracker
	std::mutex fishLock;

	//Mats for image processing
	Mat frameRaw, frameProc;
	Mat output, fgMask, frame;

	//Main pi cam
	PiCamera cam;

	cam.options->video_width = VIDEO_WIDTH;
	cam.options->video_height = VIDEO_HEIGHT;
	cam.options->verbose = 1;
	cam.options->list_cameras = true;
	cam.options->framerate = 100;
	cam.startVideo();

	fishTrackerObj.init(VIDEO_WIDTH, VIDEO_HEIGHT);
	fishTrackerObj.setRange(Scalar(BALL_H_MIN, BALL_S_MIN, BALL_V_MIN), Scalar(BALL_H_MAX, BALL_S_MAX, BALL_V_MAX));

	//Video playback
	char escKey = '\0';	
	while (escKey != 27)
	{		
		//Load video frame, if timeout, restart while loop
		if(!cam.getVideoFrame(frameRaw,1000))
		{
            cout << "Timeout error\n";
			continue; // Restart while loop
        }

		//Remove bg, run tracker, do image processing
		thread trackingThread(&FishTracker::run, fishTrackerObj, ref(frameRaw), ref(frameProc), ref(fishLock), ref(fishCount), ref(ROIRects));
		trackingThread.join();

		//Display rects
		for (auto roi : ROIRects)
		{
			rectangle(frameRaw, roi, Scalar(255, 0, 255), 2);
		}

		//Display information
		string fishDisplayStr = "Current fish: " + to_string(fishCount);
		putText(frameRaw, fishDisplayStr, Point(40, 40), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 255), 2);

		//Display video
		imshow("Video", frameRaw);
		escKey = waitKey(1);
	}	
	
	//End session with opencv
	cam.stopVideo();
	destroyAllWindows();	
}
