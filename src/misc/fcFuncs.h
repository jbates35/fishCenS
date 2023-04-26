#pragma once

#include <iostream>
#include <vector>
#include <string>

namespace _fcfuncs 
{
    void parseDateTime(std::string timestamp, std::string &date, std::string &time);
    
    std::string getTimeStamp();
    std::string getDate();

    double millis();
    
    void writeLog(std::vector<std::string> &logger, std::string data, bool outputToScreen = false);
    int saveLogFile(std::vector<std::string> &logger, std::string loggerFilename="log.txt", std::string loggerPathStr="./logData/");

    int getVideoEntry(std::string &selectionStr, std::string videoPath = "./testVideos/");
    void showVideoList(std::vector<std::string> videoFileNames, int page);

    //Definitions
    const int VIDEOS_PER_PAGE = 10;
}