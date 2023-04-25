#include "fcFuncs.h"

#include <string>
#include <sstream>
#include <chrono>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <filesystem>

using namespace cv;
using namespace std;

namespace fs = std::filesystem;

void _fcfuncs::parseDateTime(std::string timestamp, std::string &date, std::string &time)
{
    //Parse up to first three underscores
    string tempDate = "";
    string tempTime = "";

    stringstream datess(timestamp);
    string token;

    int i = 0;

    while(getline(datess, token, '_'))
    {
        if(i < 3)
        {
            tempDate += token;

            if(i < 2) tempDate += "-";
        }
        else 
        {
            tempTime = token;
        }

        i++;
    }

    stringstream timess(tempTime);

    getline(timess, token, 'h');
    tempTime = token + ":";

    getline(timess, token, 'm');
    tempTime += token + ":";

    getline(timess, token, 's');
    tempTime += token;

    date = tempDate;
    time = tempTime;
}

std::string _fcfuncs::getTimeStamp()
{
    	//Create unique timestamp for folder
	stringstream timestamp;

	//First, create struct which contains time values
	time_t now = time(0);
	tm *ltm = localtime(&now);

	//Store stringstream with numbers
	timestamp << 1900 + ltm->tm_year << "_";

	//Zero-pad month
	if ((1 + ltm->tm_mon) < 10)
	{
		timestamp << "0";
	}

	timestamp << 1 + ltm->tm_mon << "_";

	//Zero-pad day
	if ((1 + ltm->tm_mday) < 10)
	{
		timestamp << "0";
	}

	timestamp << ltm->tm_mday << "_";

	//Zero-pad hours
	if (ltm->tm_hour < 10)
	{
		timestamp << "0";
	}

	timestamp << ltm->tm_hour << "h";

	//Zero-pad minutes
	if (ltm->tm_min < 10)
	{
		timestamp << "0";
	}

	timestamp << ltm->tm_min << "m";

	//Zero-pad seconds
	if (ltm->tm_sec < 10)
	{
		timestamp << "0";
	}

	timestamp << ltm->tm_sec << "s";

	//Return string version of ss
	return timestamp.str();
}

std::string _fcfuncs::getDate()
{
    	//Create unique timestamp for folder
	stringstream timestamp;

	//First, create struct which contains time values
	time_t now = time(0);
	tm *ltm = localtime(&now);

	//Store stringstream with numbers
	timestamp << 1900 + ltm->tm_year << "_";

	//Zero-pad month
	if ((1 + ltm->tm_mon) < 10)
	{
		timestamp << "0";
	}

	timestamp << 1 + ltm->tm_mon << "_";

	//Zero-pad day
	if ((1 + ltm->tm_mday) < 10)
	{
		timestamp << "0";
	}

	timestamp << ltm->tm_mday;

	//Return string version of ss
	return timestamp.str();
}

double _fcfuncs::millis()
{
    return 1000 * getTickCount() / getTickFrequency();
}

void _fcfuncs::writeLog(std::vector<std::string> &logger, std::string data, bool outputToScreen)
{
    string dataStr = _fcfuncs::getTimeStamp() + ":\t" + data + "\n";

	if (logger.size() > 5000)
	{
		logger.erase(logger.begin());
	}

	logger.push_back(dataStr);

	if (outputToScreen == true)
	{
		cout << dataStr;
	}
}

int _fcfuncs::saveLogFile(std::vector<std::string> &logger, std::string loggerFilename, std::string loggerPathStr)
{

	//Add slash if folder-path does not end in slash so filename doesn't merge with the folder it's in
	if (loggerPathStr[loggerPathStr.size() - 1] != '/')
	{
		loggerPathStr += '/';
	}

	fs::path loggerPath = loggerPathStr;

	//Check if data folder exists in first place
	if (!fs::exists(loggerPath))
	{
		string file_command = "mkdir -p " + loggerPathStr;

		//Create directory for saving logger file in
		if (system(file_command.c_str()) == -1)
		{
			cout << _fcfuncs::getTimeStamp() + "WARNING: Could not save file!! Reason: Directory does not exist and cannot be created.\n";
			return -1;
		}

		//Change permission on all so it can be read immediately by users
		file_command = "chmod -R 777 ./" + loggerPathStr;
		system(file_command.c_str());
	}

	//Now open the file and dump the contents of the logger file into it
	ofstream outLogFile(loggerPathStr + loggerFilename + ".txt");
	for (auto logLine : logger)
	{
		outLogFile << logLine;
	}
	outLogFile.close();

	return 0;
}

int _fcfuncs::getVideoEntry(std::string &selectionStr, std::string videoPath)
{
	//Prepare video path
	if (videoPath.at(videoPath.size() - 1) == '/')
	{
		videoPath.pop_back();
	}

	//This is the return variable
	int videoEntered = -1;

	//Get vector of all files
	vector<string> videoFileNames;
	for (const auto & entry : fs::directory_iterator(videoPath))///fs::directory_iterator(TEST_VIDEO_PATH))
	{
		//Only add files that are .avi
		if (entry.path().extension() == ".txt")
		{
			continue;
		}

		videoFileNames.push_back(entry.path());
	}

	//Need to return and exit to main if there are no files
	if (videoFileNames.size() == 0)
	{
		cout << "No video files in folder found in " + videoPath + "/" << endl;
		return -1;
	}

	//Variables for iterating through part of the vidoe file folder.
	int page = 0;
	int pageTotal = (videoFileNames.size() / _fcfuncs::VIDEOS_PER_PAGE) + 1;
	cout << "Page total is " << pageTotal << endl;

	cout << videoFileNames.size() << " files found: \n";

	//Shows
	_fcfuncs::showVideoList(videoFileNames, page);

	while (videoEntered == -1)
	{
		cout << "Select video. Enter \"q\" to go back. Enter \"n\" or \"p\" for more videos.\n";
		cout << "To select video, enter number associated with file number.\n";
		cout << "I.e., to select \"File 4:\t \'This video.avi\'\", you would simply enter 4.\n";
		cout << "File: ";

		string userInput;
		cin >> userInput;

		//Leave the function and go back to main
		if (userInput == "q" || userInput == "Q")
		{
			return -1;
		}

		//Next page, show video files, re-do while loop
		if (userInput == "n" || userInput == "N")
		{
			if (++page >= pageTotal)
			{
				page = 0;
			}
			_fcfuncs::showVideoList(videoFileNames, page);
			continue;
		}

		//Previous page, show video files, re-do while loop
		if (userInput == "p" || userInput == "P")
		{
			if (--page < 0)
			{
				page = pageTotal - 1;
			}
			_fcfuncs::showVideoList(videoFileNames, page);
			continue;
		}

		//Need to make sure the string can be converted to an integer
		bool inputIsInteger = true;
		for (int charIndex = 0; charIndex < (int) userInput.size(); charIndex++)
		{
			inputIsInteger = isdigit(userInput[charIndex]);

			if (!inputIsInteger)
			{
				break;
			}
		}

		if (!inputIsInteger)
		{
			cout << "Input must be an integer between 0 and " + to_string(videoFileNames.size() - 1) + " (inclusive)" << endl;
		}

		int userInputInt = stoi(userInput);

		if (userInputInt < 0 || userInputInt >= (int) videoFileNames.size())
		{
			cout << "Input must be an integer between 0 and " + to_string(videoFileNames.size() - 1) + " (inclusive)" << endl;
			continue;
		}

		videoEntered = userInputInt;
	}

	selectionStr = videoFileNames[videoEntered];
	return 1;
}

void _fcfuncs::showVideoList(std::vector<std::string> videoFileNames, int page)
{
    	//Get indexing numbers
	int vecBegin = page * _fcfuncs::VIDEOS_PER_PAGE;
	int vecEnd = (page + 1) * _fcfuncs::VIDEOS_PER_PAGE;

	//Make sure we don't list files exceeding array size later
	if (vecEnd > (int) videoFileNames.size())
	{
		vecEnd = videoFileNames.size();
	}

	cout << "Showing files " << vecBegin << "-" << vecEnd << " of a total " << videoFileNames.size() << " files.\n";

	//Iterate and list file names
	for (int fileIndex = vecBegin; fileIndex < vecEnd; fileIndex++)
	{
		//This gives us the full path, which is useful later, but we just need the particular file name
		//Therefore, following code delimits with the slashbars
		stringstream buffSS(videoFileNames[fileIndex]);
		string currentFile;

		while (getline(buffSS, currentFile, '/'))
		{
		}

		cout << "\t>> File " << fileIndex << ":\t \"" << currentFile << "\"\n";
	}
}
