#include "Depth.h"
#include <pigpio.h>
#include <ctime>
#include <sstream>

using namespace std;

Depth::Depth() 
{
}

Depth::~Depth()
{
	serClose(_uart);
	gpioTerminate();
	//	GetTime();
	//	Distdata.open(filename, ios::app);
	//	Distdata << DateTimeStr << ", " << _depthResult << ",\n";
	//	Distdata.close();
}


int Depth::init()
{
	_uart = serOpen(SER_PORT, BAUD_RATE, 0);
	if (_uart < 0) {
		cout << "Uart Failed\n";
		return -1;
	}
	gpioDelay(UART_DELAY); //Delay to wait for serOpen to return handle

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
	int bytesRead;
	int attempts = 0; //Breaks while loop if proper data is not received in 4 attempts
	int header = 0xff; //Header byte from sensor
	

	while (attempts < 4) 
	{    
		cout << "Taking in depth data...\n";
	    
		char data[4] = { 0, 0, 0, 0 }; //Data received from ultrasonic sensor (header, data, data, and checksum bytes)
	    
		while ((serDataAvailable(_uart)) <= 3) 
		{
		}

		bytesRead = serRead(_uart, data, 4); //Reads bytes in serial data and places in data[] array
	    
		if (data[0] == header) 
		{
			//Checks first byte matches expected header value
			if (((data[0] + data[1] + data[2]) & 0x00ff) == data[3]) 
			{
				//Checks data against 'checksum' byte
				distance = (data[1] << 8) + data[2]; //Calculates distance in mm if data is good
				break;                                                  //Breaks While loop
			}
			else 
			{
				//If data is not good it increments attempts
				attempts++;
			}
		}
	}
	
	if (attempts == 4) 
	{
		depthResult = 9999;
		cerr << "\nUnstable Data";
		return -1;
	}
	
	//clog << "\n" << distance[0];
	
	//Store result from parent class, need lock so no over-writing
	{
		std::lock_guard<mutex> guard(lock);
		depthResult = distance;	
	}
	return 1;
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