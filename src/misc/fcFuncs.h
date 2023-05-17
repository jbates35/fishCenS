#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

namespace _fcfuncs 
{
    void parseDateTime(std::string timestamp, std::string &date, std::string &time);
    
    std::string getTimeStamp();
    std::string getDate();

    double millis();
    
    void writeLog(std::vector<std::string> &logger, std::string data, bool outputToScreen = false);
    int saveLogFile(std::vector<std::string> &logger, std::string loggerFilename="log.txt", std::string loggerPathStr="./logData/");

    int getVideoEntry(std::string &selectionStr, std::string videoPath = "./vid/");
    void showVideoList(std::vector<std::string> videoFileNames, int page);

    void convertRects(std::vector<cv::Rect> &rects, double scaleFactor = 1);
    void convertRect(cv::Rect &rect, double scaleFactor = 1);

    //Definitions
    const int VIDEOS_PER_PAGE = 10;
}