//
// Created by sahil on 04/05/23.
//

#ifndef DEHAZE_DEHAZE_H
#define DEHAZE_DEHAZE_H


#define BUFFER_SIZE 5


#include <opencv2/opencv.hpp>
#include <opencv2/xphoto/white_balance.hpp>
#include <chrono>
#include "cvui.h"


using namespace std;
using namespace cv;


class dehaze {

private:


    Mat _dehazed_img, _scaled_down_img, _scaled_down_dehazed_img, _negative_img, _scaled_down_negative_img, _corrected_img;
    float factor = 0;
    uint8_t alpha = 20, beta = 20 , gamma = 1;

    double _min_luminance = 85.0; // Minimum luminance value
    double BkfactorMax = 1.4; // Maximum factor value

   // CircularBuffer<std::array<uint8_t ,BUFFER_SIZE>>
   //
   // ;
    // bool ishazy(Mat &src , int roi_size, double clear_video_threshold = 7);
    double compute_luminance(const cv::Mat& input, const cv::Rect& roi, bool std_dev = false );
    Mat negative_correction(const cv::Mat& input, double factor);
    static Mat compute_dark_channel(const Mat &input, int patch_size);
    double compute_min_luminance(cv::Mat input);


public:
    void dehaze_img (Mat &src, Mat &dst, double factor_step ,float initial_factor = 0.1, bool color_correction = false, int scale_factor = 1);
    void dehaze_demo(VideoCapture &cap, double factor_step ,float initial_factor = 0.1);
    bool is_hazy(const Mat &input, int patch_size = 100 , double threshold = 200 );


    // getter and setter for min_luminance
    double get_min_luminance() const;
    void set_min_luminance(double min_luminance);

    // getter and setter for BkfactorMax
    double get_BkfactorMax() const;
    void set_BkfactorMax(double BkfactorMax);
    void photodemo(Mat const src);


    };


#endif //DEHAZE_DEHAZE_H
