#include "sqlManager.h"

using namespace _fcDatabase;

sqlManager::sqlManager()
{
}

sqlManager::~sqlManager()
{
    if(C->is_open())
    {
        C->disconnect();
        cout << "Closed database successfully: " << C->dbname() << endl;
    }
}

void sqlManager::init()
{
    _isConnected=false;

    C = new pqxx::connection("dbname=fcdb user=fishcens password=fishcens hostaddr=127.0.0.1 port=5432");

    if(C->is_open())
    {
        _isConnected=true;
        cout << "Opened database successfully: " << C->dbname() << endl;
    }
    else
    {
        cout << "Can't open database" << endl;
    }
}

void sqlManager::insertFishCounterData(_fcDatabase::fishCounterData data)
{
    if(!C->is_open())
    {
        cout << "Can't open database" << endl;
        return;
    }

    int x1 = data.roi.tl().x;
    int y1 = data.roi.tl().y;
    int x2 = data.roi.br().x;
    int y2 = data.roi.br().y;

    string query = "INSERT INTO fish_counter "
                "(fish_date, fish_time, fish_filename, direction, roi) "
                "VALUES ('" + data.date + "', '" + data.time + "', '" + data.filename + "', '" + data.direction + "',"
                "(box(point(" + to_string(x1) + ", " + to_string(y1) + "), point(" + to_string(x2) + ", " + to_string(y2) + "))))";

    W = new pqxx::work(*C);

    W->exec(query);
    W->commit();
}   

void sqlManager::insertSensorData(_fcDatabase::sensorEntry data)
{
    if(!C->is_open())
    {
        cout << "Can't open database" << endl;
        return;
    }

    string query = "INSERT INTO sensor_readings "
                "(reading_date, reading_time, water_temp, depth) "
                "VALUES ('" + data.date + "', '" + data.time + "', " + to_string(data.temperature) + ", " + to_string(data.depth) + ")";

    W = new pqxx::work(*C);

    W->exec(query);
    W->commit();
}