#include "Temperature.h"
#include <fstream>
#include <iostream>
#include <ctime>
#include <string>
#include <sstream>

using namespace std;

Temperature::Temperature() {

    GetTime();
    filename = DateTimeStr + "_Temperature.txt";
    Tempdata.open(filename, ios::out);
    Tempdata << "time, temperature,\n";
    Tempdata.close();
}

void Temperature::GetTemperature() {

    string TemperatureRaw;

    ifstream file("/sys/bus/w1/devices/28-00000dfce719/w1_slave");
    if (file.is_open()) {
        string line;
        while(getline(file, line)) {
            if(line.find("t=") != string::npos) {
                istringstream(line.substr(29)) >> TemperatureRaw;
                break;
            }
        }
        file.close();
    }
    else {
        cerr << "Can't open file" << endl;
    }

    string TemperatureStr = to_string(stof(TemperatureRaw) / 1000);

    GetTime();
    Tempdata.open(filename, ios::app);
    Tempdata << DateTimeStr + "," + TemperatureStr + ",\n";
    Tempdata.close();
}

void Temperature::GetTime() {

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
