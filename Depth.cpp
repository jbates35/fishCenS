#include "Depth.h"
#include <pigpio.h>
#include <fstream>
#include <iostream>
#include <ctime>
#include <sstream>

using namespace std;
//Location of serial port modual
char serPort[12] = {'/','d','e','v','/','t','t','y','A','M','A','0'};
//Constructor creates filenamed with current data and time
Depth::Depth() {

    GetTime();                              //Sets DateTimeStr to current time
    filename = DateTimeStr + "_Depth.txt";  //Sets filename
    Distdata.open(filename, ios::out);      //Creates file
    Distdata << "time, Depth,\n";           //Labels file's columns
    Distdata.close();                       //Closes file
}
//Records the current depth data from sensor to file
void Depth::getDepth() {

    int distance = 0;               //Distance to water
    char data[4] = {0, 0, 0, 0};    //Data received from ultrasonic sensor (header, data, data, and checksum bytes)
    int bytesRead;
    int attempts = 0;               //Breaks while loop if proper data is not received in 4 attempts
    int header = 0xff;              //Header byte from sensor

    gpioInitialise();                       //Initialize PIGPIO session
    int Uart = serOpen(serPort, 9600, 0);   //Opens serial connection at serPort with 9600 baudrate
    gpioDelay(1000);                        //Delay to wait for serOpen to return handle

    while(true) {
        while((serDataAvailable(Uart)) <= 3) {}                         //Waits for 4 bytes to be available at serial buffer

        bytesRead = serRead(Uart, data, 4);                             //Reads bytes in serial data and places in data[] array

        if(data[0] == header) {                                         //Checks first byte matches expected header value
            if(((data[0] + data[1] + data[2])&0x00ff) == data[3]) {     //Checks data against 'checksum' byte
                distance = (data[1] << 8) + data[2];                    //Calculates distance in mm if data is good
                break;                                                  //Breaks While loop
            }
            else {                                                      //If data is not good it increments attempts
            attempts++;
            }
        }
        if(attempts > 4) {                                              //If 4 attempts failed it breaks while loop
        break;
        }
        attempts++;                                                     //Increments attempts if header does not match expected value
    }

    serClose(Uart);                                                     //Close Uart connection
    gpioTerminate();                                                    //Ends PIGPIO session
    clog << "\n" << distance;                                           //Outputs distance value
    GetTime();                                                          //Sets DateTimeStr to current time
    Distdata.open(filename, ios::app);                                  //Opens log file to append with new distance and time
    Distdata << DateTimeStr << ", " << distance << ",\n";               //Write distnace and time
    Distdata.close();                                                   //Close file
}
//Sets the DateTimeStr to current date and time
void Depth::GetTime() {

    time_t start = time(nullptr);                       //Grabs current time
    tm *local_time = localtime(&start);                 //Converts time to local time in a tm struct

    stringstream DateTime;                              //Creates a stringstream object
    DateTime << (local_time->tm_year + 1900) << '_'     //Adds data from tm struct to DateTime object
            << (local_time->tm_mon + 1) << '_'
            << local_time->tm_mday << '_'
            << local_time->tm_hour << '_'
            << local_time->tm_min << '_'
            << local_time->tm_sec;
    DateTimeStr = DateTime.str();                       //Places value from DateTime object into a string
}
