#include "videoRecord.h"

VideoRecord::VideoRecord()
{
}

VideoRecord::~VideoRecord()
{ 
}

void VideoRecord::run(Mat& frame, mutex& lock)
{
	//Mutex and record frame
	lock_guard<mutex> guard(lock);
	
	//Exit function if video isn't open
	if (!_video.isOpened())
	{
		cout << "Video is not opened\n";
		_log("Video is not open");
		return;
	}
	
	_video.write(frame);	
	
	//Update timer information
	if (_frameTimes.size() > MAX_DATA_SIZE)
	{
		_frameTimes.erase(_frameTimes.begin());
	}
	
	_frameTimes.push_back(getTickCount() / getTickFrequency() - _frameTimer);
	_frameTimer = getTickCount() / getTickFrequency();
	
	//Update frame count
	_frameCount++;
}

void VideoRecord::init(Mat& frame, mutex& lock, double fps, string filePath /* = NULL */)
{	
	//Mutex and record frame
	lock_guard<mutex> guard(lock);
	
	//Setup filepath and filenames, first
	if (filePath.compare(NULL))
	{
		_filePath = "./";
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
	int codec = VideoWriter::fourcc('M', 'J', 'P', 'G');
	bool isColor = (frame.type() == CV_8UC3);
	Size videoSize = frame.size();

	_log("Starting video.");
	_log("Frame rate is " + to_string(fps) + "fps,\t meaning period is " + to_string((double) 1000 / fps) + "ms");
	_log("Video size is <" + to_string(videoSize.width) + ", " + to_string(videoSize.height));
	
	cout << "Starting video\n";
	cout << "FPS is " + to_string(fps) + "\nMeaning period is " + to_string((double) 1 / fps) + "\n";
	
	//Start video timer
	_startTime = getTickCount() / getTickFrequency();
	
	//Start frame counter and frame time
	_frameCount = 0;
	_frameTimer = getTickCount() / getTickFrequency();
	
	//Initailize video
	_video.open(_filePath + _fileName + ".avi", codec, fps, videoSize, isColor);
	
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

void VideoRecord::_log(string data)
{
	//Clear first entry in the vector if buffer size is full
	if (_loggerVec.size() > MAX_DATA_SIZE)
	{
		_loggerVec.erase(_loggerVec.begin());
	}
	
	_loggerVec.push_back(_getTime() + ": " + data + "\n");
}


