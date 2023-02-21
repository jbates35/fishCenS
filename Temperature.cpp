#include "Temperature.h"
#include <fstream>
#include <iostream>
#include <ctime>
#include <string>
#include <sstream>

using namespace std;
//Constructor creates log file
Temperature::Temperature() {

    GetTime();                                      //Sets DateTimeStr to current time
    filename = DateTimeStr + "_Temperature.txt";    //Sets log file name
    Tempdata.open(filename, ios::out);              //Creates log file
    Tempdata << "time, temperature,\n";             //Labels columns of log file
    Tempdata.close();                               //Closes log file
}
//Gets current temperature from sensor and records in log file
void Temperature::GetTemperature() {

    string line;                                                    //Stores line string from one-wire file
    string TemperatureRaw;                                          //Stores just the numerical temperature value from one-wire file

    ifstream file("/sys/bus/w1/devices/28-00000dfce719/w1_slave");  //Opens location where Pi stores one-wire device data
    if (file.is_open()) {                                           //Checks file opened correctly
        while(getline(file, line)) {                                //Loops as long as device is connected and Temperature string can be gottten from file location
            if(line.find("t=") != string::npos) {                   //Checks there is a valid temperature reading
                istringstream(line.substr(29)) >> TemperatureRaw;   //Extracts just the numerical portion of temperature string
                break;                                              //Breaks loop
            }
        }
        file.close();                                               //Closes one-wire file
    }
    else {
        cerr << "Can't open file" << endl;                          //Error output, usually when one-wire hasn't been initialised on Pi or sensor is incorrectly connected
    }

    string TemperatureStr = to_string(stof(TemperatureRaw) / 1000); //Converts temperature string to float and calculates temp im degrees Celsius

    GetTime();                                                      //Sets DateTimeStr to current time
    Tempdata.open(filename, ios::app);                              //Opens log file to append
    Tempdata << DateTimeStr + "," + TemperatureStr + ",\n";         //Appends log file with curent time and temperature
    Tempdata.close();                                               //Closes log file
}

void Temperature::GetTime() {

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
