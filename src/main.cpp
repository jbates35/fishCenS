#include <iostream>
#include <pigpio.h>
#include <sstream>
#include <unistd.h>

#include "fishCenS.h"

void printHelp();

int main(int argc, char *argv[])
{

	//Parse through command line arguments
    bool displayOn = false;
    bool sensorsOff = false;
    bool ledOff = false;
    int mode = 0;

    int opt;
    while ((opt = getopt(argc, argv, "dslhm:")) != -1) 
	{
        switch (opt) 
		{
            case 'd':
                displayOn = true;
				std::cout << "Display on\n";
                break;
            case 's':
                sensorsOff = true;
				std::cout << "Sensors off\n";
                break;
            case 'l':
                ledOff = true;
				std::cout << "LED off\n"; 
                break;
			case 'h':
				printHelp();
				return 0;
            case 'm':
                mode = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-d] [-s] [-l] [-m mode]" << std::endl;
                return 1;
        }
    }

	//initiate pigpio
	if (gpioInitialise() < 0)
	{
		cout << "Cannot find pigpio, exiting...\n";
		return -1;
	}
	
	cout << "GPIO initialized and open\n\n";

	FishCenS fc;
	int initSuccess = false;

	switch (mode)
	{
	//Tracking
	case 0:
		std::cout << "Mode selected: Tracking\n";
		initSuccess = fc.init(fcMode::TRACKING);
		break;
		
	//Calibration
	case 1:
		std::cout << "Mode selected: Calibration\n";
		initSuccess = fc.init(fcMode::CALIBRATION);
		break;
		
	//Tracking with video test
	case 2:								
		std::cout << "Mode selected: Tracking with video\n";
		initSuccess = fc.init(fcMode::TRACKING_WITH_VIDEO);
		break;
		
		//Tracking with video test
	case 3:								
		std::cout << "Mode selected: Calibration with video\n";
		initSuccess = fc.init(fcMode::CALIBRATION_WITH_VIDEO);
		break;
		
		//Tracking with video test
	case 4:								
		std::cout << "Mode selected: Video recorder\n";
		initSuccess = fc.init(fcMode::VIDEO_RECORDER);
		break;
						
	default:
		//Loop or quit
		break;
	}			
	
	if(initSuccess>0)
	{
		if(displayOn)
			fc.displayOn();

		if(sensorsOff)
			fc.sensorsOff();

		if(ledOff)
			fc.ledOff();
			
		//fc.setTesting(true);
		fc.run();
	}

	fc.closePeripherals();
	gpioTerminate();
	
	return 0;
}

void printHelp()
{
	std::cout << "Usage: fishCenS [-d] [-s] [-l] [-m mode]" << std::endl;
	std::cout << "\t-d\t\tDisplay on" << std::endl;
	std::cout << "\t-s\t\tSensors off" << std::endl;
	std::cout << "\t-l\t\tLED off" << std::endl;
	std::cout << "\t-m mode\t\tMode" << std::endl;
	std::cout << "\t\t\t0: Tracking" << std::endl;
	std::cout << "\t\t\t1: Calibration" << std::endl;
	std::cout << "\t\t\t2: Tracking with video (TESTING)" << std::endl;
	std::cout << "\t\t\t3: Calibration with video (TESTING)" << std::endl;
	std::cout << "\t\t\t4: Video (TESTING)" << std::endl;
}