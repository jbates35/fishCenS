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
}

using namespace _depth;

class Depth {

private:

    string DateTimeStr;
    string filename;
    fstream Distdata;
    void GetTime();
	int _uart;
	int _depthResult;

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