#pragma once

#include <pigpio.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <mutex>

using namespace std;

namespace _depth
{
	char SER_PORT[12] = { '/', 'd', 'e', 'v', '/', 't', 't', 'y', 'A', 'M', 'A', '0' };
	int UART_DELAY = 50; //ms
	int BAUD_RATE = 9600;
}

using namespace _depth;

class Depth {

private:

 	string DateTimeStr;     //Value set by GetTime method can be used by other methods
	string filename;        //File created by constructor will be written to by getDepth
	fstream Distdata;       //fstream object to write to log file
	void GetTime();         //Sets DateTimeStr to current time
	int _uart;				//handle for UART

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
	
};