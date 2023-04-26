#pragma once

#include <iostream>
#include <pqxx/pqxx>

#include <vector>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

namespace _fcDatabase
{
    struct sensorEntry
    {
        int id;
        string date;
        string time;
        double depth;
        double temperature;
    };

    struct fishCounterData
    {
        int id;
        string date;
        string time;
        string filename;
        char direction;
        Rect roi;
    };
}

class sqlManager
{
public:
    sqlManager();
    ~sqlManager();

    void init();

    void insertFishCounterData(_fcDatabase::fishCounterData data);
    void insertSensorData(_fcDatabase::sensorEntry data);

    bool isConnected() { return _isConnected; }

private:
    pqxx::connection *C;
    pqxx::work *W;
    bool _isConnected;
};