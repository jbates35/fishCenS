#include "gui.h"

#define CVUI_IMPLEMENTATION
#define CVUI_DISABLE_COMPILATION_NOTICES
#include "cvui.h"

#define WINDOW1_NAME "Windows 1"
#define WINDOW2_NAME "Windows 2"
#define SIZE Size(1920,1080)
#define IMG_SIZE Size(1422,847)
#define TEXT_SIZE 0.55
#define TEXT_COLOR 0xffffff

void MatType( Mat inputMat )
{
    int inttype = inputMat.type();

    string r, a;
    uchar depth = inttype & CV_MAT_DEPTH_MASK;
    uchar chans = 1 + (inttype >> CV_CN_SHIFT);
    switch ( depth ) {
        case CV_8U:  r = "8U";   a = "Mat.at<uchar>(y,x)"; break;
        case CV_8S:  r = "8S";   a = "Mat.at<schar>(y,x)"; break;
        case CV_16U: r = "16U";  a = "Mat.at<ushort>(y,x)"; break;
        case CV_16S: r = "16S";  a = "Mat.at<short>(y,x)"; break;
        case CV_32S: r = "32S";  a = "Mat.at<int>(y,x)"; break;
        case CV_32F: r = "32F";  a = "Mat.at<float>(y,x)"; break;
        case CV_64F: r = "64F";  a = "Mat.at<double>(y,x)"; break;
        default:     r = "User"; a = "Mat.at<UKNOWN>(y,x)"; break;
    }
    r += "C";
    r += (chans+'0');
    cout << "Mat is of type " << r << " and should be accessed with " << a << endl;

}

gui::gui()
{
}

int gui::init(FishTracker& fishTrackerObj)
{
    frame1 = cv::Mat(SIZE.height, SIZE.width, CV_8UC3);
    cv::namedWindow(WINDOW1_NAME);
    cvui::init(WINDOW1_NAME);
    
    // // Initilize values
    // erode.value = fishTrackerObj.getErode();
    // dialate.value = fishTrackerObj.getDilate();
    // hsv_min.value = fishTrackerObj.getRange(RANGE_MIN);
    // hsv_max.value = fishTrackerObj.getRange(RANGE_MAX);


    // Erode trackbar pram
    erode.min.height = 1;
    erode.max.height = 50;
    erode.min.width = 1;
    erode.max.width = 50;

    // Dialate trackbar pram
    dialate.min.height = 1;
    dialate.max.height = 50;
    dialate.min.width = 1;
    dialate.max.width = 50;

    // Dialate trackbar pram
    hsv_min.min = Scalar(0,0,0);
    hsv_min.max = Scalar(255,255,255);
    hsv_max.min = Scalar(0,0,0);
    hsv_max.max = Scalar(255,255,255);

    return 1;
}

void gui::_gui(Mat& img,FishTracker &fishTrackerObj){


    cvui::context(WINDOW1_NAME);
    // Fill the frame with a nice color

    frame1 = cv::Scalar(49, 52, 49);

    padding = 500;

    resize(img,img,IMG_SIZE);

    //resize(frameDraw,frameDraw,frameDraw.size()/2);

   // resize(img,img,img.size()/2);


    cvui::beginColumn(frame1,0,0);
        cvui::image(img);


    cvui::endColumn();


    cvui::beginColumn(frame1, frame1.rows + 400, 30,padding);
        cvui::trackbar(400, &erode.value.height,erode.min.height,erode.max.height);
        string s = "Erode Height";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%30s",s.c_str());
        cvui::trackbar(400, &erode.value.width,erode.min.width,erode.max.width);
        s = "Erode Width";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%30s",s.c_str());
        // fishTrackerObj.setErode(erode.value);

        cvui::trackbar(400, &dialate.value.height,dialate.min.height,dialate.max.height);
        s = "Dialate Height";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%30s",s.c_str());

        cvui::trackbar(400, &dialate.value.width,dialate.min.width,dialate.max.width);
        s = "Dialate Width";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%30s",s.c_str());
        // fishTrackerObj.setDilate(dialate.value);



        // HSV
        cvui::trackbar(400, &hsv_min.value(0),hsv_min.min(0),hsv_min.max(0));
        s = "H Min";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%25s",s.c_str());

        cvui::trackbar(400, &hsv_min.value(1),hsv_min.min(1),hsv_min.max(1));
        s = "S Min";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%25s",s.c_str());

        cvui::trackbar(400, &hsv_min.value(2),hsv_min.min(2),hsv_min.max(2));
        s = "V Min";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%25s",s.c_str());


        cvui::trackbar(400, &hsv_max.value(0),hsv_min.min(0),hsv_min.max(0));
        s = "H Max";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%25s",s.c_str());

        cvui::trackbar(400, &hsv_max.value(1),hsv_min.min(1),hsv_min.max(1));
        s = "S Max";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%25s",s.c_str());

        cvui::trackbar(400, &hsv_max.value(2),hsv_min.min(2),hsv_min.max(2));
        s = "V Max";
        cvui::printf(TEXT_SIZE, TEXT_COLOR,"%25s",s.c_str());



        // fishTrackerObj.setRange(hsv_min.value, hsv_max.value);


        // // TO do -> add Export button -> add date and time to .json as default
        // if (cvui::button(frame1, 1625, 670,100, 30, "EXPORT"))
        //         imex_object.Export(fishTrackerObj, _gui::LOGGER_PATH);

    cvui::endColumn();
    cvui::update(WINDOW1_NAME);
    
    cv::imshow(WINDOW1_NAME, frame1);


}
