#include <iostream>
#include <pigpio.h>
#include <sstream>
#include <unistd.h>

#include "fishCenS.h"

int main(int argc, char *argv[])
{

	//Parse through command line arguments
    bool displayOn = false;
    bool sensorsOff = false;
    bool ledOff = false;
    int mode = 0;

    int opt;
    while ((opt = getopt(argc, argv, "dslm:")) != -1) 
	{
        switch (opt) 
		{
            case 'd':
                displayOn = true;
                break;
            case 's':
                sensorsOff = true;
                break;
            case 'l':
                ledOff = true;
                break;
            case 'm':
                mode = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-d] [-s] [-l] [-m mode]" << std::endl;
                return 1;
        }
    }

    std::cout << "displayOn = " << displayOn << std::endl;
    std::cout << "sensorsOff = " << sensorsOff << std::endl;
    std::cout << "ledOff = " << ledOff << std::endl;
    std::cout << "mode = " << mode << std::endl;

	//initiate pigpio
	if (gpioInitialise() < 0)
	{
		cout << "Cannot find pigpio, exiting...\n";
		return -1;
	}
	
	cout << "GPIO initialized and open\n\n";

	char menuKey = '\0';

	while (menuKey != '0')
	{
		cout << "Choose option:\n\n";
		cout << "\t>> (1) Tracking\n";
		cout << "\t>> (2) Calibration\n";
		cout << "\t>> (3) Tracking with video (TESTING)\n";
		cout << "\t>> (4) Calibration with video (TESTING)\n";
		cout << "\t>> (5) Video (TESTING)\n";
		cout << "\t>> (0) Quit\n\n";
		cout << "Choose key: ";

		//Get user input:
		menuKey = getchar();

		{
			FishCenS fc;
			int initSuccess = false;

			switch (mode)
			{
			//Tracking
			case 0:
				initSuccess = fc.init(fcMode::TRACKING);
				break;
				
			//Calibration
			case 1:
				initSuccess = fc.init(fcMode::CALIBRATION);
				break;
				
			//Tracking with video test
			case 2:								
				initSuccess = fc.init(fcMode::TRACKING_WITH_VIDEO);
				break;
				
				//Tracking with video test
			case 3:								
				initSuccess = fc.init(fcMode::CALIBRATION_WITH_VIDEO);
				break;
				
				//Tracking with video test
			case 4:								
				initSuccess = fc.init(fcMode::VIDEO_RECORDER);
				break;
								
			default:
				//Loop or quit
				break;
			}			
			
			if(initSuccess>0)
			{
				//fc.setTesting(true);
				fc.run();
			}
		}
	}
	
	gpioTerminate();
	
	return 0;
}



