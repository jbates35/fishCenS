#pragma once

#include <pigpio.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <mutex>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

namespace _depth
{
	const int UART_DELAY = 500; //ms
	const int BAUD_RATE = 9600;
	const double UART_TIMEOUT = 1.0; //in seconds, timeout for reading uart bus
}

using namespace _depth;

class Depth {

private:

 	string DateTimeStr;     //Value set by GetTime method can be used by other methods
	string filename;        //File created by constructor will be written to by getDepth
	fstream Distdata;       //fstream object to write to log file
	void GetTime();         //Sets DateTimeStr to current time
	int _uart;				//handle for UART
	bool _timeOut;
	double _startTimer;	

public:

	/**
	 * @brief Constructor, does nothing (call init for constructor)
	 **/
    Depth();
	
	/**
	 * @brief CLoses file and logs distance
	 **/
	~Depth();
	
	/**
	 * @brief Processes serial communication from ultrasonic sensor to get distance
	 * @return -1 if error, 1 if success
	 **/
    int getDepth(int& depthResult, mutex& lock);
	
	/**
	 * @brief initializes depth object
	 * @return -1 if error, 1 if success
	 **/
	int init();
	
	/**
	 * @brief Getter for distance
	 * @return Distance value after calculated
	 **/
	int distance();
	
	/**
	* @brief Opens serial, gets depth, closes serial
	* @param depthResult pointer to the result of the depth, gets updated when valid
	* @param lock mutex pointer
	*/
	void run(int& depthResult, mutex& lock);

};