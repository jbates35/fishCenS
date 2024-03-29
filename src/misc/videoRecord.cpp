#include "videoRecord.h"

#include <filesystem>
#include "misc/fcFuncs.h"

namespace fs = std::filesystem;
using namespace _vr;

VideoRecord::VideoRecord()
{
	//Make sure video folder exists and change permissions to 777
	if (!fs::exists(VIDEO_PATH))
	{
		fs::create_directory(VIDEO_PATH);
		fs::permissions(VIDEO_PATH, fs::perms::all);
	}
}

VideoRecord::~VideoRecord()
{ 
	if(_vrStatus != vrMode::VIDEO_OFF)
	{
		close();
	}
}

void VideoRecord::run(Mat& frame, mutex& lock)
{		
	{	
		scoped_lock lockGuard (lock);
		_isInRunFunc = true;
	}
	
	//Exit function if video isn't open
	if (!_video.isOpened())
	{
		cout << "Video is not opened\n";
		_log("Video is not open");
		return;
	}
	
	Mat writeFrame;
		
	//Mutex and record frame	
	{
		scoped_lock lockGuard(lock);
		writeFrame = frame.clone();
	}	
	
	
	// Write frame to video
	_video.write(writeFrame);	
	
	//Update timer information
	if (_frameTimes.size() > MAX_DATA_SIZE)
	{
		_frameTimes.erase(_frameTimes.begin());
	}
	
	_frameTimes.push_back(getTickCount() / getTickFrequency() - _frameTimer);
	_frameTimer = getTickCount() / getTickFrequency();
	
	//Update frame count
	_frameCount++;
		
	{	
		scoped_lock lockGuard(lock);
		_isInRunFunc = false;
	}
}

void VideoRecord::init(Mat& frame, mutex& lock, double fps, string filePath /* = NULL */)
{	
	_vrStatus = vrMode::VIDEO_SETUP;
	_isInRunFunc = false;
	
	//Setup filepath and filenames, first
	if (filePath == "")
	{
		_filePath = VIDEO_PATH;
	}
	else
	{
		_filePath = filePath;
		
		//Append slash if there isn't one at end of string
		if (_filePath.at(_filePath.size() - 1) != '/')
		{
			_filePath += '/';
		}
	}
	
	//Make timestamp filename
	_fileName = _getTime();

	//Create video parameters
	Size videoSize;
	bool isColor;
	int codec = VideoWriter::fourcc('M', 'J', 'P', 'G');
	{
		scoped_lock guard(lock);
		isColor = (frame.type() == CV_8UC3);
		videoSize = frame.size();
	}
	
	_log("Starting video...", true);
	_log("\t>>Video name is " + _fileName, true);
	_log("\t>>Theoretical frame rate is " + to_string(fps) + "fps", true);
	_log("\t>>Theoretical frame length is " + to_string((double) 1000 / fps) + "ms", true);
	_log("\t>>Camera size is <" + to_string(videoSize.width) + ", " + to_string(videoSize.height) + "> px", true);
	
	//Start video timer
	_startTime = getTickCount() / getTickFrequency();
	
	//Start frame counter and frame time
	_frameCount = 0;
	_frameTimer = getTickCount() / getTickFrequency();
	
	//Initiailize video
	{		
		scoped_lock guard(lock);
		_video.open(_filePath + _fileName + ".avi", codec, fps, videoSize, isColor);
	}
	_vrStatus = vrMode::VIDEO_ON;
	
}

void VideoRecord::close()
{
	_vrStatus = vrMode::VIDEO_OFF;

	//Save video file to file
	_video.release();

	cout << "Video closed\n";
	_log("Closing video...");
	_log("Video length: " + to_string(getTickCount() / getTickFrequency() - _startTime) + "s");
	
	//Get mean of frame times
	if (_frameCount > 1)
	{
		double frameMeanTime = (getTickCount() / getTickFrequency() - _startTime) / _frameCount;
	
		_log("Mean frame time: " + to_string(frameMeanTime * 1000) + "ms");
		_log("Mean frame rate: " + to_string(1 / frameMeanTime) + "fps");	
		
		//Now get standard deviation
		double frameSDSquared = 0;
	
		for (auto frameTime : _frameTimes)
		{
			frameSDSquared += (frameTime - frameMeanTime)*(frameTime - frameMeanTime);
		}
		double frameSD = sqrt(frameSDSquared / (_frameCount - 1.0));
	
		_log("Variance (standard deviation) of frame time: " + to_string(frameSD * 1000) + "ms");
	}
	
	//Make text file and dump data
	ofstream dataFile(_filePath + _fileName + ".txt");
	
	for (auto str : _loggerVec)
	{
		dataFile << str;
	}

	dataFile.close();

}

vrMode VideoRecord::isOpen()
{
	return _vrStatus;
}


// Will be true if the run function is currently running
bool VideoRecord::isInRunFunc()
{
	return _isInRunFunc;
}

std::string VideoRecord::_getTime()
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

void VideoRecord::_log(string data, bool outputToScreen)
{
	string dataStr = _getTime() + ": " + data + "\n";

	//Clear first entry in the vector if buffer size is full
	if (_loggerVec.size() > MAX_DATA_SIZE)
	{
		_loggerVec.erase(_loggerVec.begin());
	}
	
	_loggerVec.push_back(dataStr);

	if(outputToScreen==true)
	{
		cout << dataStr;
	}
}


