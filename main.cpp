#include <iostream>
#include <pigpio.h>


#include "fishCenS.h"


int main(int argc, char *argv[])
{

	//initiate pigpio
	if (gpioInitialise() < 0)
	{
		cout << "Cannot find pigpio, exiting...\n";
		return -1;
	}

	char menuKey = '\0';

	while (menuKey != '0')
	{
		cout << "Choose options\n";
		cout << "(1) Tracking";
		cout << "(2) Calibration\n";
		cout << "(3) Tracking with video (TESTING)\n";
		cout << "(4) Calibration with video (TESTING)\n";
		cout << "(5) Video (TESTING)\n";
		cout << "(0) Quit\n";

		//Get user input:
		menuKey = getchar();

		{
			FishCenS fc;
			
			switch (menuKey)
			{
			//Tracking
			case '1':
				fc.init(fcMode::TRACKING);
				break;
				
			//Calibration
			case '2':
				fc.init(fcMode::CALIBRATION);
				break;
				
			//Tracking with video test
			case '3':								
				fc.init(fcMode::TRACKING_WITH_VIDEO);
				break;
				
				//Tracking with video test
			case '4':								
				fc.init(fcMode::CALIBRATION_WITH_VIDEO);
				break;
				
				//Tracking with video test
			case '5':								
				fc.init(fcMode::VIDEO_RECORDER);
				break;
								
			default:
				//Loop or quit
				break;
			}			
			
			fc.run();
		}
	}
	
	return 0;
}



