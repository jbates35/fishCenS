#include "imex.h"

using namespace  cv;


imex::imex(){
    string_prams[prams::dialate] = "Dialate";
    string_prams[prams::erode] = "Erode";
    string_prams[prams::margin] = "Dialate";
    string_prams[prams::frame_center] = "Margin";
    string_prams[prams::track_frames] = "Frame_center";
    string_prams[prams::min_com_rect_area] = "Min_combined_RectArea";
    string_prams[prams::roi_scale] = "Roi_scale";
    string_prams[prams::range_min] = "Range_Min";
    string_prams[prams::range_max] = "Range_Max";
    string_prams[prams::rect_track_region] = "Tracking_Region";
}

imex::~imex(){

}


bool imex::import(String filepath ,String name , FishTracker& fishtracker)
{
    std::string file = filepath + "/" + name + ".json";
    try {
        cv::FileStorage  fs(file,cv::FileStorage::READ);
        if (!fs.isOpened()) {
            std::cout << "cannot open " << file << " for read" << std::endl;
            return false;
        }


        // Get type from getters for setters
        // auto dialate = fishtracker.getDilate();
        // auto erode  = fishtracker.getErode();
        // auto margin = fishtracker.getMargin();
        // auto frame_center = fishtracker.getFrameCenter();
        // auto rect_track_frame = fishtracker.getRetrackFrames();
        // auto roi_scale = fishtracker.getROIScale();
        // auto range_min = fishtracker.getRange(RANGE_MIN);
        // auto range_max = fishtracker.getRange(RANGE_MAX);
        // auto rect_track_region = fishtracker.getRetrackRegion();
        // auto min_com_rect_area = fishtracker.getMinCombinedRectArea();

        // Get vals from the file
        // fs [string_prams[prams::dialate]] >> dialate;
        // fs [string_prams[prams::erode]]  >> erode;
        // fs [string_prams[prams::margin]] >> margin;
        // fs [string_prams[prams::frame_center]] >> frame_center;
        // fs [string_prams[prams::rect_track_region]] >> rect_track_frame;
        // fs [string_prams[prams::min_com_rect_area]]  >> min_com_rect_area;
        // fs [string_prams[prams::roi_scale]] >>  roi_scale;
        // fs [string_prams[prams::range_min]] >> range_min;
        // fs [string_prams[prams::range_max]] >> range_max;
        // fs [string_prams[prams::rect_track_region]] >> rect_track_region;


        // Set values from file
        // fishtracker.setDilate(dialate);
        // fishtracker.setErode(erode);
        // fishtracker.setMargin(margin);
        // fishtracker.setFrameCenter(frame_center);
        // fishtracker.setROIScale(roi_scale);
        // fishtracker.setMinCombinedRectArea(min_com_rect_area);
        // fishtracker.setRetrackFrames(rect_track_frame);
        // fishtracker.setRetrackRegion(rect_track_region);
        // fishtracker.setRange(range_min,RANGE_MIN);
        // fishtracker.setRange(range_max,RANGE_MAX);
        // cout << fishtracker.getErode() << endl;


    } catch (...) {
        std::cout << "cannot open .json for read" << std::endl;
        return false;
    }

     return true;
}


bool imex::Export( FishTracker fishtracker, String filepath ,String name)
{
    if(name == "default"){
        auto now = std::chrono::system_clock::now();
        //auto UTC = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream datetime;
        datetime << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");

        name = datetime.str();

    }


    std::string file = filepath + "/" + name + ".json";
    try {
        cv::FileStorage  fs(file,cv::FileStorage::WRITE);
        if (!fs.isOpened()) {
            std::cout << "cannot open " << file << " for write" << std::endl;
            return false;
        }
        // fs << string_prams[prams::dialate] << fishtracker.getDilate();
        // fs << string_prams[prams::erode]  << fishtracker.getErode();
        // fs << string_prams[prams::margin] << fishtracker.getMargin();
        // fs << string_prams[prams::frame_center] << fishtracker.getFrameCenter();
        // fs << string_prams[prams::min_com_rect_area]  << fishtracker.getMinCombinedRectArea();
        // fs << string_prams[prams::roi_scale]<< fishtracker.getROIScale();
        // fs << string_prams[prams::range_min]<< fishtracker.getRange(RANGE_MIN);
        // fs << string_prams[prams::range_max]<< fishtracker.getRange(RANGE_MAX);
        // fs << string_prams[prams::rect_track_region]<< fishtracker.getRetrackRegion();
        fs.release();

    } catch (...) {
        std::cout << "cannot open .json for write" << std::endl;
        return false;
    }
    return true;
}



