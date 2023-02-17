#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <fstream>
#include <string>

using namespace std;

class Temperature {

    private:

        string filename;
        string DateTimeStr;
        fstream Tempdata;
        void GetTime();

    public:

        Temperature();
        void GetTemperature();
};

#endif // TEMPERATURE_H
