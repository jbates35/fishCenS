import cv2 as cv
import numpy as np
import os
import pathlib
import sys

from pycoral.adapters import common
from pycoral.adapters import detect
from pycoral.utils.dataset import read_label_file
from pycoral.utils.edgetpu import make_interpreter

# Specify the TensorFlow model, labels, and image
script_dir = pathlib.Path(__file__).parent.absolute()
model_file = os.path.join(script_dir, 'models/FishCenS_FPN_edgetpu.tflite')

# Initialize the TF interpreter
interpreter = make_interpreter(model_file)
interpreter.allocate_tensors()

testing = False
print("Interpreter loaded")

def fishML(mat):
    # Crop the image taking only an roi of a square in the center
    height, width, ztotal = mat.shape
    
    height_larger = height > width
    frame_diff = int(abs(height-width)/2)
   
    height_diff = 0
    width_diff = 0 
    
    if height_larger and not height==width:
        black_img = np.zeros((height, frame_diff, ztotal), np.uint8)
        mat = np.concatenate((black_img, mat, black_img), axis=1)
        height_diff = 0
        width_diff = frame_diff
        
    elif not height_larger and not height==width:
        black_img = np.zeros((frame_diff, width, ztotal), np.uint8)
        mat = np.concatenate((black_img, mat, black_img), axis=0)
        height_diff = frame_diff
        width_diff = 0

    # center_x = int(width/2)
    # center_y = int(height/2)
    # roi_size = min(width, height)
    
    # x = center_x - int(roi_size/2)
    # y = center_y - int(roi_size/2)
    # mat = mat[y:y+roi_size, x:x+roi_size]

    mat = cv.cvtColor(mat, cv.COLOR_BGR2RGB)
    
    size = common.input_size(interpreter)
    _, scale = common.set_resized_input(interpreter, mat.shape[:2], lambda size: cv.resize(mat, size))
    
    # Run an object detection
    interpreter.invoke()    
    objs = detect.get_objects(interpreter, 0.4, scale)

    # Create an empty list to store the rois
    returnRects = []
    
    # Print the result
    if not objs:
        if testing:
            print ("No objects detected")
        returnRects.append([-1, -1, -1, -1, -1])
        return returnRects
        
    for obj in objs:        
        # Get the bounding box
        if testing:
            print ('x = ', obj.bbox.xmin, 'y = ', obj.bbox.ymin, 'w = ', obj.bbox.width, 'h = ', obj.bbox.height, 'score = ', obj.score)
            
        if obj.score > 0.68:
            xmin, ymin, xmax, ymax = obj.bbox
            
            returnRects.append([
                xmin - width_diff, 
                ymin - height_diff, 
                (xmax-xmin), 
                (ymax-ymin),
                obj.score
                ]) 
            
    return returnRects  
