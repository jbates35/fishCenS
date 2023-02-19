#include "Depth.h"
#include <pigpio.h>
#include <fstream>
#include <iostream>
#include <ctime>
#include <sstream>

using namespace std;
char serPort[12] = {'/','d','e','v','/','t','t','y','A','M','A','0'};

Depth::Depth() {

    GetTime();
    filename = DateTimeStr + "_Depth.txt";
    Distdata.open(filename, ios::out);
    Distdata << "time, Depth,\n";
    Distdata.close();
}

void Depth::getDepth() {

    int distance[2] = {0, 0};
    char data[4] = {0, 0, 0, 0};
    int bytesRead;
    int i = 0;
    int attempts = 0;

    gpioInitialise();
    int Uart = serOpen(serPort, 9600, 0);
    if(Uart < 0) {
        cout << "\nUart Failed";
    }
    else {
        cout << "\nUart Good";
    }
    while(true) {
        cout << "\nEnter Loop";
        while((serDataAvailable(Uart)) <= 3) {}

        bytesRead = serRead(Uart, data, 4);
        if(bytesRead < 0) {
            cerr << "\nError reading data";
        }
    
        distance[i] = (data[1] * 256) + data[2];

        if(i >= 1) {
            if(abs(distance[0] - distance[1]) < 50) {
                break;
            }
            else {
                cerr << "\nattempt failed";
                i = 0;
                attempts++;
            }
        }

        if(attempts >= 3) {
            distance[0] = 9999;
            cerr << "\nUnstable Data";
            break;
        }
        i++;
    }

    serClose(Uart);
    gpioTerminate();
    clog << "\n" << distance[0];
    GetTime();
    Distdata.open(filename, ios::app);
    Distdata << DateTimeStr << ", " << distance[0] << ",\n";
    Distdata.close();
}

void Depth::GetTime() {

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
