#ifndef TEMPERATURE_H
#define TEMPERATURE_H

#include <fstream>
#include <string>

#include <mutex>

using namespace std;
namespace Temperature
{
	const string TEMP_DEVICE = "28-00000dfce719";
	void getTemperature(double& temperature, mutex& lock);  
}

using namespace Temperature;

#endif // TEMPERATURE_H

