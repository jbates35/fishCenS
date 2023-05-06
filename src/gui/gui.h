#pragma once



#include <opencv2/opencv.hpp>
#include "../tracking/fishTracker.h"
#include "imex.h"

namespace _gui
{
  const string LOGGER_PATH = "./logData"; // Folder path for Logging data
}

using namespace cv;


class gui
{
public:
    gui();
    int init(FishTracker& fishTrackerObj);
    void _gui(Mat& img, FishTracker &fishtrackerobj);

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

};






