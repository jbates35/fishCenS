#include "fishMLWrapper.h"
#include <fstream>
#include <string>

FishMLWrapper::FishMLWrapper()
{
}

FishMLWrapper::~FishMLWrapper()
{
   //Clean up python interpreter
   Py_XDECREF(_pROI);
   Py_XDECREF(_pVal);
   Py_XDECREF(_pArgs);
   Py_XDECREF(_pReturn);
   Py_XDECREF(_pImg);
   Py_XDECREF(_pFunc);
   Py_XDECREF(_pModule);
   Py_Finalize();
}

int FishMLWrapper::init()
{
   //Open python file fishML.py as file
   ifstream infile("fishML.py");
   vector<string> pyCommands;
   string line;

   //Parse through the contents of the file and store in a vector of strings
   while(getline(infile, line))
   {
      //Find any "#" character and trim the line to that point
      size_t pos = line.find("#");
      if (pos != string::npos)
      {
         line = line.substr(0, pos);
      }

      //Continue if line is empty or contains only spaces
      if (line.empty() || line.find_first_not_of(' ') == string::npos)
      {
         continue;
      }

      //Break if line is a function definition
      if (line.find("def") != string::npos)
      {
         break;
      }

      pyCommands.push_back(line);
   }

   //Initialize python interpreter
   Py_Initialize();

   //Parse through python commands
   for (string command : pyCommands)
   {
      PyRun_SimpleString(command.c_str());
   }

   if (_import_array() < 0) 
   {
      PyErr_Print(); 
      PyErr_SetString(PyExc_ImportError, "numpy.core.multiarray failed to import");
   }
  
   //Set folder path to python script
   PySys_SetPath(L".");

   //Import python script
   _pModule = PyImport_ImportModule("fishML");

   //Make sure python script was imported
   if (!_pModule)
   {
      PyErr_Print();
      return -1;
   }

   //Get python function from script
   _pFunc = PyObject_GetAttrString(_pModule, "fishML");

   //Make sure python function was imported
   if (!_pFunc || !PyCallable_Check(_pFunc))
   {
      PyErr_Print();
      return -1;
   }

   return 0;
}

int FishMLWrapper::update(Mat &srcImg, vector<FishMLData> &objData)
{
   if(srcImg.empty())
   {
      cout << "No source image for update" << endl;
      return -1;
   }

   //Convert Mat to numpy array
   npy_intp dims[3] = { srcImg. rows, srcImg.cols, srcImg.channels() };
   _pImg = PyArray_SimpleNewFromData(srcImg.dims + 1, (npy_intp*) & dims, NPY_UINT8, srcImg.data);

   //Check GIL state
   if(!PyGILState_Check())
   {
      //Release GIL and save thread state
      PyThreadState* state = PyEval_SaveThread();

      //Re-acquire GIL and restore thread state
      PyGILState_STATE gilState = PyGILState_Ensure();

      //Create python argument list
      _pArgs = PyTuple_New(1);
      PyTuple_SetItem(_pArgs, 0, _pImg);

      //Create return value pointer
      _pReturn = PyObject_CallObject(_pFunc, _pArgs);

      //Release GIL
      PyGILState_Release(gilState);

      //Restore thread state
      PyEval_RestoreThread(state);

      //Make sure there's a return value
      if (_pReturn == NULL)
      {
         PyErr_Print();
         return -1;
      }

      //Parse return value to vector of Rects
      //First declare variables needed
      vector<FishMLData> tempObjData;

      //Get size of return value
      Py_ssize_t rSize = PyList_Size(_pReturn);

      //Iterate through return value
      for (Py_ssize_t i = 0; i < rSize; i++)
      {
         //This stores the iterated items which will form the rects later
         _pROI = PyList_GetItem(_pReturn, i);
         vector<int> rectVals;

         //Parse through to get the rect coordinates
         for (Py_ssize_t j = 0; j < 4; j++)
         {
            // Get object of value, convert it to a c++ style int, and store it in the vector
            _pVal = PyList_GetItem(_pROI, j);
            int val = PyLong_AsLong(_pVal);
            rectVals.push_back(val);
         }
         //Get ID
         _pVal = PyList_GetItem(_pROI, 4);
         double score = PyFloat_AsDouble(_pVal);

         //Create Rect from vector and dump for later
         Rect ROI(rectVals[0], rectVals[1], rectVals[2], rectVals[3]);

         FishMLData data = { ROI, score };

         tempObjData.push_back(data);
      }

      //Clear ROIs and store new ROIs
      objData.clear();

      //If no ROIs were found, return 0
      if(tempObjData.size() == 1 && tempObjData[0].ROI.x == -1)
      {
         return 0;
      }
      
      //Success, dump ROIs
      objData = tempObjData;
   
   }

   return 1;
}
