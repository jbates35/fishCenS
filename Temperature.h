#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <fstream>
#include <string>

using namespace std;

class Temperature {

    private:

        string filename;        //Stores file name of log file
        string DateTimeStr;     //Holds time and date of the last time GetTime method was called
        fstream Tempdata;       //fstream object to open log file
        void GetTime();         //Sets DateTimeStr to current date and time

    public:

        Temperature();          //Constructor creates log file
        void GetTemperature();  //Gets current temp value and time and records it to log file
};

#endif // TEMPERATURE_H

