#pragma once



#include <opencv2/opencv.hpp>
#include "fishTracker.h"
#include "imex.h"


#define FOLDER_PATH "/home/sahil/cproj/opencv/" // Folder path for your Pi
#define LOGGER_PATH "/home/sahil/cproj/opencv/exportdata/" // Folder path for Logging data

using namespace cv;


class gui
{
public:
    gui(FishTracker fishTrackerObj);
    void _gui(Mat img, FishTracker &fishtrackerobj);

private:
    // Create variables used by some components
    std::vector<double> window1_values;
    std::vector<double> window2_values;
    bool window1_checked = false, window1_checked2 = false;
    bool window2_checked = false, window2_checked2 = false;
    double _erode_val = 1.0, window1_value2 = 1.0, window1_value3 = 1.0;
    Mat frame1;
    int padding;
    imex imex_object;

    template <typename T>
    struct range {
      T min , max, value;
    };

    range<Size> erode, dialate;
    //range<Scalar_<double>> hsv_min, hsv_max;
    range<Scalar> hsv_min, hsv_max;
    range<float> margin, roi_scale;
    range<int> track_region, track_frames, min_threshold_area, comb_rect_area, frame_center;
    bool init = true; 

};






