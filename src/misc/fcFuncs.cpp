#include "fcFuncs.h"
#include <string>
#include <sstream>

using namespace std;

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