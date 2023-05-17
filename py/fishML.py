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
model_file = os.path.join(script_dir, 'models/efficientdet-lite-fishcens_edgetpu.tflite')

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
            
        if obj.score > 0.55:
            xmin, ymin, xmax, ymax = obj.bbox
            
            returnRects.append([
                xmin - width_diff, 
                ymin - height_diff, 
                (xmax-xmin), 
                (ymax-ymin),
                obj.score
                ]) 
            
    return returnRects  

# import time
# from picamera2 import Picamera2

# def play_video():
#     cwd = os.getcwd()
#     #cap = cv.VideoCapture(cwd + '/vid/fish_dark_trimmed_2.mov')

#     #Open camera
#     picam = Picamera2()
#     picam.configure(picam.create_preview_configuration(main={"format": 'XRGB8888', "size": (640, 480)}))
#     picam.start()
            
#     # # Create an OpenCV VideoCapture object
#     # cap = cv.VideoCapture(picam, cv.CAP_GSTREAMER)

#     # # Check if the camera opened successfully
#     # if not cap.isOpened():
#     #     print("Failed to open the camera")
#     #     exit()

#     while True:

#         #Start timer
#         startTimer = time.time()

#         # # Read frame from camera
#         # ret, frame = cap.read()
        
#         frame = picam.capture_array()
#         # frame = shrink_to_fit_square(frame)      
        
#         endTimer = time.time()
#         if testing:
#             print("Time to read frame: ", 1000*(endTimer - startTimer), "ms")

#         # if not ret:
#         #     print("Failed to read frame from the camera")
#         #     break    
            
#         startTimer = time.time()
        
#         # Run the object detection on the frame
#         objects_found = fishML(frame)
        
#         endTimer = time.time()
        
#         if testing:
#             print("Time to detect objects: ", 1000*(endTimer - startTimer), "ms")
        
#         # Display the detection results on the frame
#         startTimer = time.time()

#         for x, y, w, h, score in objects_found:
#             if(x == -1):
#                 break

#             # Draw a bounding box around the detected object
#             cv.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
#             cv.putText(frame, f'{"Fish"}:{score:.2f}', (x, y-5), cv.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

#         # Display the frame
#         cv.imshow('Object Detection', frame)
        
#         # Exit the loop if the 'q' key is pressed
#         if cv.waitKey(1) == ord('q'):
#             break
        
#         endTimer = time.time()
#         if testing:
#             print("Time to display frame: ", 1000*(endTimer - startTimer), "ms")

#     # Release the video capture object and close the display window
#     #cap.release()
#     cv.destroyAllWindows()  

# def play_picture():
#     # Define the key variable assigned to waitKey
#     key = ''
    
#     abs_path = os.path.dirname(__file__)
#     rel_path = 'img/griffin2.png'
#     full_path = os.path.join(abs_path, rel_path)
    
#     img = cv.imread(full_path)
#     img_clone = img.copy()
    
#     returnList = fishML(img)
#     print(returnList)
        
#     while(key != ord('q')):
#         # Parse through the list of faces
#         for objInfo in returnList:
#             x = objInfo[0]
#             y = objInfo[1]
#             w = objInfo[2]
#             h = objInfo[3]            
#             cv.rectangle(img, (x, y), (x+w, y+h), (0, 255, 0), 2)
#             cv.putText(img, "Score: " + str(objInfo[4]), (x+10, y+40), cv.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)
        
#         # Display the image
#         cv.imshow('Image', img)

#         # Wait for a key press
#         key = cv.waitKey(0)

#     cv.destroyAllWindows()
    
    
# #If we are in main program
# if __name__ == "__main__":
#     play_video()
