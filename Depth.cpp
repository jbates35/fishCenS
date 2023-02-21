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
	if (serOpen(SER_PORT, 9600, 0) < 0) {
		cout << "Uart Failed\n";
		return -1;
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
    
    int bytesRead;
    int i = 0;
    int attempts = 0;
	int distance[2] = { 0, 0 };
	

    while(attempts<3) {
	    
	    cout << "Taking in depth data...\n";
	    
	    char data[4] = { 0, 0, 0, 0 };
	    
	    while ((serDataAvailable(_uart)) <= 3) {}

	    bytesRead = serRead(_uart, data, 4);
        if(bytesRead < 0) {
            cerr << "\nError reading data";
        }
    
	    distance[i] = (data[1] * 256) + data[2];

        if(i >= 1) {
	        if (abs(distance[0] - distance[1]) < 50) {
                break;
            }
            else {
                cerr << "\nattempt failed";
                i = 0;
                attempts++;
            }
        }
	    
        i++;
    }
	
	if (attempts == 3) {
		_depthResult = 9999;
		cerr << "\nUnstable Data";
		return -1;
	}
	
	clog << "\n" << distance[0];
	
	//Store result from parent class, need lock so no over-writing
	{
		std::lock_guard<mutex> guard(lock);
		
		//Store member variable and then store it to parent variable
		_depthResult = distance[0];
		depthResult = distance[0];	
	}
	return 1;
}

void Depth::GetTime() 
{
    time_t start = time(nullptr);
    tm *local_time = localtime(&start);

    stringstream DateTime;
    DateTime << (local_time->tm_year + 1900) << '_'
            << (local_time->tm_mon + 1) << '_'
            << local_time->tm_mday << '_'
            << local_time->tm_hour << '_'
            << local_time->tm_min << '_'
            << local_time->tm_sec;
    DateTimeStr = DateTime.str();
}