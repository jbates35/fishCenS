//
// Created by sahil on 04/05/23.
//

#define CVUI_DISABLE_COMPILATION_NOTICES
#define CVUI_IMPLEMENTATION
#include "dehaze.h"


double medianMat(cv::Mat Input){
    Input = Input.reshape(0,1); // spread Input Mat to single row
    std::vector<double> vecFromMat;
    Input.copyTo(vecFromMat); // Copy Input Mat to vector vecFromMat
    std::nth_element(vecFromMat.begin(), vecFromMat.begin() + vecFromMat.size() / 2, vecFromMat.end());
    return vecFromMat[vecFromMat.size() / 2];
}
double dehaze:: compute_min_luminance(cv::Mat input) {
    double mean_luminance = compute_luminance(input, Rect()) / input.total();
    double threshold = 1000.0 / (1.0 + exp((50.0 - mean_luminance) / 20.0)); // logistic function
    cout << "threshold: " << threshold << endl;
    double min_luminance = std::max(50.0, threshold);
    return min_luminance;
}
// Compute the dark channel of an input image
cv::Mat dehaze:: compute_dark_channel(const cv::Mat& input, int patch_size) {
    // Convert input to grayscale
    cv::Mat gray;
    cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);

    // Compute the minimum value in local patches
    cv::Mat dark_channel;
    cv::erode(gray, dark_channel, cv::Mat::ones(patch_size, patch_size, CV_8UC1));

    return dark_channel;
}

// Compute the luminance of an image
double dehaze:: compute_luminance(const cv::Mat& input, const cv::Rect& roi, bool std_dev ) {
    // Convert input image to CIE 1931 XYZ color space
    cv::Mat xyz;
    cv::cvtColor(input, xyz, cv::COLOR_BGR2XYZ);

    // Split XYZ image into its channels
    std::vector<cv::Mat> channels;
    cv::split(xyz, channels);


    // If std_dev is false, compute mean of Y channel in the ROI
    if ( std_dev == false ) {
        // Compute mean of Y channel in the ROI
        cv::Mat luminance = channels[1]; // Y channel in XYZ color space
        double mean_luminance = cv::mean(luminance)[0]; // Compute mean luminance value
        return mean_luminance;
    }
    // Compute standard deviation of Y channel in the ROI
    cv::Mat luminance = channels[1](roi); // Y channel in XYZ color space
    cv::Scalar mean, stddev;
    cv::meanStdDev(luminance, mean, stddev);

    return stddev.val[0];
}


/*
double dehaze:: compute_luminance(const cv::Mat& img,const cv::Rect& roi, bool std_dev) {
    // Compute minimum value across all color channels
    cv::Mat min_channel;
    cv::reduce(img, min_channel, 2, cv::REDUCE_MIN);

    // Compute luminance from minimum channel
    double luminance = cv::mean(min_channel)[0];

    return luminance;
}
*/



// Check if an image is hazy using the dark channel prior
 bool  dehaze::  is_hazy(const cv::Mat& input, int patch_size , double threshold ) {
    // Compute the dark channel
    cv::Mat dark_channel = compute_dark_channel(input, patch_size);

    // Compute the mean and standard deviation of the dark channel
    cv::Scalar mean, stddev;
    cv::meanStdDev(dark_channel, mean, stddev);
    cout << "stddev:" <<stddev[0] << endl;

    // Check if the standard deviation is below the threshold
    return stddev[0] > threshold;
}


/*// Detect if the video is hazy or not by computing the standard deviation of the luminance channel in the center of the image
// Less accurate than dark channel prior but faster to compute
// Threshold to be 5 - 10 for clear video
bool dehaze::ishazy(Mat &src, int roi_size, double clear_video_threshold ) {

    int center_x = src.cols / 2;
    int center_y = src.rows / 2;
    cv::Rect roi(center_x - roi_size / 2, center_y - roi_size / 2, roi_size, roi_size);
    double luminance_stdev = compute_luminance(src, roi,true);
    cout << "Luminance Stdev: " << luminance_stdev << endl;
    if (luminance_stdev < clear_video_threshold) {
       return true;
    }
    return false;
}*/

Mat dehaze:: negative_correction(const cv::Mat& input, double factor) {
    // Compute negative image
    cv::Mat negative;
    cv::bitwise_not(input, negative);

    // Apply negative correction
    cv::Mat output = input - factor * negative;

    // Clip output values to [0, 255]
    cv::threshold(output, output, 0.0, 255.0, cv::THRESH_TOZERO);
    cv::threshold(output, output, 255.0, 255.0, cv::THRESH_TRUNC);

    return output;
}

void dehaze:: dehaze_img (Mat &src, Mat &dst, double factor_step ,float initial_factor,bool color_corr, int scaled_factor) {


    if (scaled_factor > 1) {
        resize(src, _corrected_img, Size(), 1.0 / scaled_factor, 1.0 / scaled_factor, INTER_LINEAR);
    }else{
        src.copyTo(_corrected_img);
    }

/*    Ptr<xphoto::WhiteBalancer> wb = xphoto::createGrayworldWB();
    wb->balanceWhite(_corrected_img, _corrected_img);*/

    // Get the negative of the image
    bitwise_not(_corrected_img,_negative_img);

    double luminance = compute_luminance(_corrected_img,Rect(Point(1,3),Size(3,2)),false);// Compute luminance of input frame

    // Make min lumanance such that its agressive if the image is bright and less agressive if the image is dark


    _min_luminance = compute_min_luminance(_corrected_img);
    double old_luminance = luminance;

    //cout << "Luminance: " << luminance << endl;
    //cout << "Min Luminance: " << _min_luminance << endl;


    factor = initial_factor; // Initial factor value
    cv::Mat corrected_img = _corrected_img.clone();
    while (luminance > _min_luminance && factor < BkfactorMax) {
        corrected_img= negative_correction(_corrected_img, factor); // Apply dehazing
        double corrected_luminance = compute_luminance(corrected_img,Rect()); // Compute corrected luminance

        if (corrected_luminance <= luminance) {
            luminance = corrected_luminance;
        }
        else {
            break;
        }

        factor += factor_step; // Increment factor
    }
    if(luminance < _min_luminance && factor > factor_step && old_luminance > _min_luminance){
        factor -= factor_step;
    }


    if(scaled_factor != 1){
        corrected_img= negative_correction(src, factor);
    }
    if(color_corr){
        Ptr<xphoto::WhiteBalancer> wb = xphoto::createGrayworldWB();
        wb->balanceWhite(corrected_img, corrected_img);
    }


    corrected_img.copyTo(dst);

    // use the gamma  beta and alpha values to adjust the brightness and contrast of the image
    //dst.convertTo(dst, -1, 1.1, 10);


    cout << "Factor: " << factor << endl;
    cout << "Luminance: " << luminance << endl;

    // Update buffer with the current factor
}


void dehaze:: dehaze_demo(VideoCapture &cap, double factor_step ,float initial_factor){

    Mat frame, dst;
    bool paused = false, color_correction = false;
    cap >> frame;
    cv::namedWindow("Comparison");
    LowPassFilter filter(0.1);

    cv::Mat comparison_frame(frame.rows + 145,frame.cols, CV_8UC3);
    comparison_frame = ::Scalar(49, 52, 49);

    cv::Mat mask(frame.size(), CV_8UC1, cv::Scalar(128));
    cv::Mat inv_mask(frame.size(), CV_8UC1, cv::Scalar(128));

    cvui::init("Comparison");

    cv::Mat concat_frame;
    int padding = 50;
    int slider_position = 50; // initial slider position
    int computation_time = 0;

    while (true) {

        // Update the frame if the video is not paused
        if (!paused)
        {

            cap >> frame;

            if (frame.empty()) {
                break;
            }

            auto start = std::chrono::high_resolution_clock::now();
            if(is_hazy(frame, 10, 20)){

                dehaze_img(frame, dst, factor_step, initial_factor,color_correction,10);
                cv::putText(frame, "Hazy", Point(50, 50), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 0, 255), 2);
            }
            else {
                frame.copyTo(dst);
                cv::putText(frame, "Clear", Point(50, 50), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);
            }
            auto finish = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<std::chrono::milliseconds>(finish - start);
            computation_time = duration.count();

        }

        // Grey frame
        comparison_frame = ::Scalar(49, 52, 49);

        // put the computation time on the frame on top right corner
        cv::putText(dst, "Computation time: " + to_string(computation_time) + " ms", Point(frame.cols - 300, 30), FONT_ITALIC, 0.69, Scalar(255, 255, 255), 2);

        cv::hconcat(frame(cv::Rect(0, 0, frame.cols * slider_position / 100, frame.rows)),
                    dst(cv::Rect(frame.cols * slider_position / 100, 0, frame.cols - frame.cols * slider_position / 100, frame.rows)),
                    concat_frame);


        cvui::beginColumn(comparison_frame, 0, 0, frame.cols, frame.rows + 500, 0);
        cvui::image(concat_frame);
        cvui::endColumn();

        cvui::beginRow(comparison_frame, 0, frame.rows, frame.cols, -1, padding);
        cvui::trackbar(frame.cols, &slider_position, 1, 99);

        //cvui::checkbox("Pause", &pause, 0xFF0000);
        cvui::endRow();

        cvui::beginRow(comparison_frame, 0, frame.rows + 10 , frame.cols, -1, padding);

        //cvui::checkbox(comparison_frame, 20,20,"Pause", &paused, 0xFF0000);
        cvui::endRow();

        if (cvui::button(comparison_frame, frame.cols/2 + 60 , frame.rows + 60,100 ,50,"Pause/Play"))
        {
            paused = !paused;
        }

        if (cvui::button(comparison_frame, frame.cols/2 - 200 , frame.rows + 60,150 ,50,"Color Correction"))
        {
            color_correction = !color_correction;
        }



        // Display the trackbar



        cvui::imshow("Comparison", comparison_frame);
        cvui::update("Comparison");

        if (cv::waitKey(33) == 27) {
            break;
        }
    }



    cap.release();
    cv::destroyAllWindows();

}

// getter and setter for the min luminance
double dehaze::get_min_luminance() const {
    return _min_luminance;
}

void dehaze::set_min_luminance(double min_luminance) {
    dehaze::_min_luminance = min_luminance;
}

// getter and setter for the max factor
double dehaze::get_BkfactorMax() const {
    return BkfactorMax;
}

void dehaze::set_BkfactorMax(double BkfactorMax) {
    dehaze::BkfactorMax = BkfactorMax;
}

void dehaze:: photodemo(Mat const src){


    Mat frame = src.clone();
    Mat  comparison_frame(frame.rows + 145,frame.cols, CV_8UC3), concat_frame,dst;
    int padding = 50;
    int slider_position = 50; // initial slider position
    bool color_correction = false, paused = false;
    double factor_step = 0.05, initial_factor = 0.1, computation_time = 0;

    cv::namedWindow("Comparison");
    comparison_frame = ::Scalar(49, 52, 49);
    cvui::init("Comparison");



    while (true) {

        // Update the frame if the video is not paused
        auto start = chrono::high_resolution_clock::now();
        if(is_hazy(frame, 10, 20)){

            dehaze_img(frame, dst, factor_step, initial_factor,color_correction,10);
        }
        else {
            frame.copyTo(dst);
        }
        auto finish = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<std::chrono::milliseconds>(finish - start);
        computation_time = duration.count();



        // Grey frame
        comparison_frame = ::Scalar(49, 52, 49);

        // put the computation time on the frame on top right corner
        cv::putText(dst, "Computation time: " + to_string(computation_time) + " ms", Point(frame.cols - 300, 30), FONT_ITALIC, 0.69, Scalar(255, 255, 255), 2);

        cv::hconcat(frame(cv::Rect(0, 0, frame.cols * slider_position / 100, frame.rows)),
                    dst(cv::Rect(frame.cols * slider_position / 100, 0, frame.cols - frame.cols * slider_position / 100, frame.rows)),
                    concat_frame);


        cout << frame.size() << endl;
        cvui::beginColumn(comparison_frame, 0, 0, frame.cols, frame.rows + 500, 0);
        cvui::image(concat_frame);
        cvui::endColumn();

        cvui::beginRow(comparison_frame, 0, frame.rows, frame.cols, -1, padding);
        cvui::trackbar(frame.cols, &slider_position, 1, 99);

        //cvui::checkbox("Pause", &pause, 0xFF0000);
        cvui::endRow();

        \


        if (cvui::button(comparison_frame, frame.cols/2 - 200 , frame.rows + 60,150 ,50,"Color Correction"))
        {
            color_correction = !color_correction;
        }



        // Display the trackbar



        cvui::imshow("Comparison", comparison_frame);
        cvui::update("Comparison");

        if (cv::waitKey(2) == 27) {
            break;
        }
    }
}



