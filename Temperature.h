#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <fstream>
#include <string>

#include <mutex>

using namespace std;
namespace _temperature
{
	const string TEMP_LOCATION = "/sys/bus/w1/devices/28-00000dfce719/w1_slave";
	void getTemperature(double& temperature, mutex& lock);  
}

using namespace _temperature;

#endif // TEMPERATURE_H

