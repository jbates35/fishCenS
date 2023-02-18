#ifndef DEPTH_H
#define DEPTH_H

#include <pigpio.h>
#include <fstream>
#include <iostream>

using namespace std;

class Depth {

    private:

        string DateTimeStr;     //Value set by GetTime method can be used by other methods
        string filename;        //File created by constructor will be written to by getDepth
        fstream Distdata;       //fstream object to write to log file
        void GetTime();         //Sets DateTimeStr to current time

    public:

        Depth();                //Constructor creates log file
        void getDepth();        //Gets sensor data and writes to file created by constructor
};

#endif // DEPTH_H
