#include "Depth.h"
#include <pigpio.h>
#include <ctime>
#include <sstream>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace _depth;

Depth::Depth() 
{
}

Depth::~Depth()
{
	//serClose(_uart);

	//	GetTime();
	//	Distdata.open(filename, ios::app);
	//	Distdata << DateTimeStr << ", " << _depthResult << ",\n";
	//	Distdata.close();
}

int Depth::init()
{
	//char SER_PORT[] = { '/', 'd', 'e', 'v', '/', 't', 't', 'y', 'A', 'M', 'A', '0' }; // UNCOMMENT If PI 3 
	char serPort[] = "/dev/serial0";
	
	_uart = serOpen(serPort, BAUD_RATE, 0);
	
	gpioDelay(UART_DELAY); //Delay to wait for serOpen to return handle
	
	if (_uart < 0)
	{
		cout << "Uart Failed\n";
		return -1;
	}
	else
	{
		cout << "Uart successfully read.\n";
	}

//	
//	GetTime();
//	filename = DateTimeStr + "_Depth.txt";
//	Distdata.open(filename, ios::out);
//	Distdata << "time, Depth,\n";
//	Distdata.close();
	
	return 1;
}


int Depth::getDepth(int& depthResult, mutex& lock) 
{
    
	int distance = 0; //Distance to water
	int attempts = 0; //Breaks while loop if proper data is not received in 4 attempts
	int header = 0xff; //Header byte from sensor
	char data[4] = { 0, 0, 0, 0 }; //Data received from ultrasonic sensor (header, data, data, and checksum bytes)
	_startTimer = getTickCount() / getTickFrequency();
	_timeOut = false;
	
	while (true) {
		
		//Flush serial buffer by reading all bytes
		while (serDataAvailable(_uart) > 0) 
		{
			//Read serial byte
			serReadByte(_uart);
		}
		
		while ((serDataAvailable(_uart) <= 3) && !_timeOut)  
		{
			_timeOut = (getTickCount() / getTickFrequency() - _startTimer) > UART_TIMEOUT;
			
			if (_timeOut)
			{
				cout << "UART Timed out\n";
				return -1;
			}
		}
		
		serRead(_uart, data, 4); //Reads bytes in serial data and places in data[] array
		
		if (data[0] == header) 
		{
			//Checks first byte matches expected header value
			if (((data[0] + data[1] + data[2]) & 0x00ff) == data[3]) 
			{
				//Checks data against 'checksum' byte
				distance = (data[1] << 8) + data[2]; //Calculates distance in mm if data is good
				break;                                                  //Breaks While loop
			}
		}
		if (attempts > 4) 
		{
			//If 4 attempts failed it breaks while loop
			return -1;
		}
		attempts++; //Increments attempts if header does not match expected value
	}
	
	//clog << "\n" << distance[0];
	
	//Store result from parent class, need lock so no over-writing
	{
		std::lock_guard<mutex> guard(lock);
		depthResult = distance;	
	}
	
	//cout << "Success: Depth data stored.\nDepth: " << distance << endl;
	return 1;
}

void Depth::run(int& depthResult, mutex& lock)
{
	if (init() >= 0)
	{
		getDepth(depthResult, lock);
		serClose(_uart);
	}
}


void Depth::GetTime() 
{
	time_t start = time(nullptr);
	tm *local_time = localtime(&start);

	stringstream DateTime; //Creates a stringstream object
	DateTime << (local_time->tm_year + 1900) << '_'     //Adds data from tm struct to DateTime object
	        << (local_time->tm_mon + 1) << '_'
	        << local_time->tm_mday << '_'
	        << local_time->tm_hour << '_'
	        << local_time->tm_min << '_'
	        << local_time->tm_sec;
	DateTimeStr = DateTime.str();
}