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

enum
{
	VIDEO_OFF,
	VIDEO_ON
};

#define MAX_DATA_SIZE 10000 // Max amount of info logger can contain

class VideoRecord
{
public:
	/** @brief Initializes the video writer, i.e. fps codec size
	 * @param frame Mat passed from camera to get first shot of video
	 * @param fps FPS that video will be set to
	 * @param filePath If set, folder path that video 
	 **/
	VideoRecord(Mat& frame, double fps, string filePath = NULL);
	
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
	
	/**
	 * @brief Getter for videoWriter's isOpen
	 * @return True if video is recording
	 **/
	bool isOpen();
	
private:
	//////////// PARAMETERS ///////////
	
	//Video related
	VideoWriter _video;
	
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