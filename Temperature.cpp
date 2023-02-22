#include "Temperature.h"
#include <fstream>
#include <iostream>
#include <ctime>
#include <string>
#include <sstream>

using namespace std;

void _temperature::getTemperature(double& temperature, mutex& lock) {

    string line;                                                    //Stores line string from one-wire file
    string TemperatureRaw = "-999";                                          //Stores just the numerical temperature value from one-wire file

	ifstream file("/sys/bus/w1/devices/" + TEMP_DEVICE + "/w1_slave"); //Opens location where Pi stores one-wire device data
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
        cout << "Can't open file" << endl;                          //Error output, usually when one-wire hasn't been initialised on Pi or sensor is incorrectly connected
	    return;
    }

	{
		std::lock_guard<mutex> guard(lock);
		if (TemperatureRaw != "-999")
		{
				temperature = stod(TemperatureRaw) / 1000;
		}
	}
}

