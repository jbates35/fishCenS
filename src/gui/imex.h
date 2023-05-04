#ifndef IMEX_H
#define IMEX_H

#include "fishTracker.h"
#include <map>
#include <chrono>
class imex
{
public:
    // Import the settings from the tracker to the name json file
    bool import(String filepath ,String name, FishTracker&);

    // Export from saves setting Json file to the Tracker
    bool Export( FishTracker, String filepath ,String name = "default");

    imex();
    ~imex();

 private:

    map<int, string> string_prams;

    enum prams{
        dialate,
        erode,
        margin,
        frame_center,
        track_frames,
        min_com_rect_area,
        roi_scale,
        range_min,
        range_max,
        rect_track_region
    };

//    // To be used in case we need the performance likely not needed
//    union type {
//        Size size;
//        float float_val;
//        int int_val;
//        Scalar scalar;
//    };

//    struct pram
//    {
//        type value;
//        prams name;
//    };



};

#endif // IMEX_H
