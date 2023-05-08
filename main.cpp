


#include <iostream>
#include "functimer.h"
#include "dehaze.h"


#include <iostream>

using namespace std;
using namespace cv;






int main() {

    dehaze dehaze;
    Mat haze_img, dehazed_img;
    functimer<std::chrono::milliseconds> ft;

    string filename = "fish.avi";
    //filename = "fishk.mp4";
    //filename = "stock2.mp4";
    //filename = "fishDirect_LED.mp4";
    //filename = "stock3.mp4";
   // filename = "stock4.mp4";
    //filename = "stock5.mp4";
    //filename = "stock6.mp4";
    //filename = "hazyfish.mp4";



    VideoCapture cap(filename);



    haze_img = imread("hazy_city.jpg");
    //haze_img = imread("cars-fog.jpg");
    //haze_img = imread("blur.png");

    //haze_img = imread("hazy1.jpg");
    //haze_img = imread("fish.jpg");

    //haze_img = imread("hazyfish.jpg");

    dehaze.photodemo(haze_img);

    int roi_size = 200;

    dehaze.dehaze_img(haze_img,dehazed_img,0.1);
    dehaze.is_hazy(haze_img,roi_size);



    /*if(is_hazy(haze_img, 300, 0.2)) {
       cout << "Hazy" << endl;
       dehaze.dehaze_img(haze_img, dehazed_img,0.15);
         imshow("dehazed", dehazed_img);
         waitKey(0);
    }

    if(is_hazy(dehazed_img, 300, 40)) {
        cout << "Hazy1" << endl;
    }
*/
    dehaze.dehaze_demo(cap, 0.1,0);


    std::cout << "Hello, World!" << std::endl;
    return 0;
}
