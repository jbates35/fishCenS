#pragma once

#include <iostream>
#include <opencv2/opencv.hpp>
#include <Python.h>
#include <numpy/arrayobject.h>
#include <mutex>

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

using namespace std;
using namespace cv;

//Definitions
namespace fml
{
   const string _modelPath = "'models/fishModel1'";
   const string _labelPath = "'labels/mscoco_label_map.pbtxt.txt'";
}

using namespace fml;

//Data structure for rect, id, score
struct FishMLData
{
   Rect ROI;
   double score;
};

class FishMLWrapper
{
public:
   FishMLWrapper();
   ~FishMLWrapper();

   int init();

   /**
    * @brief Updates the ML model with the given image and returns the ROIs
    * @param srcImg The image to be processed from opencv (camera shot)
    * @param objData Vector of ROIs from parent class that get updated at end
    * @return -1 if error, 0 if nothing found, 1 if things found
   */
   int update(Mat &srcImg, vector<FishMLData> &objData);

private:
   PyObject* _pModule;
   PyObject* _pFunc;
   PyObject* _pReturn;
   PyObject* _pROI;
   PyObject* _pArgs;
   PyObject* _pVal;
   PyObject* _pImg;
};