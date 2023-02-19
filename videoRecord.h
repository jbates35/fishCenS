#pragma once

#include <thread>
#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <sstream>
#include <cstdlib>
#include <fstream>

#include <opencv2/opencv.hpp>
#include <opencv2/imgcodecs.hpp>

using namespace cv;
using namespace std;


enum vrMode
{
	VIDEO_OFF,
	VIDEO_SETUP,
	VIDEO_ON
};

#define MAX_VIDEO_DATA_SIZE 10000 // Max amount of info logger can contain

class VideoRecord
{
public:

	/**
	 * Constructor does nothing
	*/
	VideoRecord();
	
	/**
	 * Saves and closes video.
	 */
	~VideoRecord();
	
	/**
	 * @brief New thread that needs to be run every 1/fps s, taking video
	 * @param frame Frame of image that will get recorded
	 * @param lock Mutex from parent class
	 **/
	void run(Mat& frame, mutex& lock);	
	
	/** @brief Initializes the video writer, i.e. fps codec size
	 * @param frame Mat passed from camera to get first shot of video
	 * @param lock Mutex from parent class
	 * @param fps FPS that video will be set to
	 * @param filePath If set, folder path that video 
	 **/
	void init(Mat& frame, mutex& lock, double fps, string filePath = "");

	/**
	 * @brief Getter for videoWriter's isOpen
	 * @return True if video is recording
	 **/
	bool isOpen();
	
private:
	//////////// PARAMETERS ///////////
	
	//Video related
	VideoWriter _video;
	int _frameCount;
	double _frameTimer;
	vector<double> _frameTimes; 
	
	//File related
	string _fileName;
	string _filePath;
	double _startTime;
	
	//Logger related
	vector<string> _loggerVec;
	
	//////////// METHODS ///////////
	
	//Gets date&time to name video
	string _getTime();
	void _log(string data);	
};