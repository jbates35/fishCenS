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

			switch (menuKey)
			{
			//Tracking
			case '1':
				initSuccess = fc.init(fcMode::TRACKING);
				break;
				
			//Calibration
			case '2':
				initSuccess = fc.init(fcMode::CALIBRATION);
				break;
				
			//Tracking with video test
			case '3':								
				initSuccess = fc.init(fcMode::TRACKING_WITH_VIDEO);
				break;
				
				//Tracking with video test
			case '4':								
				initSuccess = fc.init(fcMode::CALIBRATION_WITH_VIDEO);
				break;
				
				//Tracking with video test
			case '5':								
				initSuccess = fc.init(fcMode::VIDEO_RECORDER);
				break;
								
			default:
				//Loop or quit
				break;
			}			
			
			if(initSuccess>0)
			{
				fc.run();
			}
		}
	}
	
	gpioTerminate();
	
	return 0;
}



