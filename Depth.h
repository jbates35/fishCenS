#ifndef DEPTH_H
#define DEPTH_H

#include <pigpio.h>
#include <fstream>
#include <iostream>

using namespace std;

class Depth {

    private:

        string DateTimeStr;
        string filename;
        fstream Distdata;
        void GetTime();

    public:

        Depth();
        void getDepth();
};

#endif // DEPTH_H
