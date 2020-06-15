import cv2
import os
import ntpath
import numpy as np

LEFT_EYE_SCHEME = "_LEFT_EYE_"
RIGHT_EYE_SCHEME = "_RIGHT_EYE_"

def UnstitchImage(frame):

    height, width, dim = frame.shape
    left_eye_frame = frame[0:height, 0:(width//2)]
    right_eye_frame = frame[0:height, (width//2):width]
        
    return left_eye_frame, right_eye_frame

def ProcessMovie(movie_path, target_dir, naming_scheme):

    # define target paths
    right_image_path = os.path.join(target_dir, naming_scheme + RIGHT_EYE_SCHEME)
    left_image_path = os.path.join(target_dir, naming_scheme + LEFT_EYE_SCHEME)

    # load movie
    movie_cap = cv2.VideoCapture(movie_path)
    
    frame_number= 0

    print("Extract frames and unstitch")
    ret = True
    while ret:
        # Capture frame-by-frame
        ret, frame = movie_cap.read()

        if ret:
            if frame_number == 0:
                total_frames = int(movie_cap.get(cv2.CAP_PROP_FRAME_COUNT))
                print("Number of frames: %d" %total_frames)

            left_eye_frame, right_eye_frame = UnstitchImage(frame)
             
            print("%s%d.jpg" %(ntpath.basename(right_image_path), frame_number))
            print("%s%d.jpg" %(ntpath.basename(left_image_path), frame_number))

            cv2.imwrite("%s%d.jpg" %(right_image_path, frame_number), right_eye_frame)
            cv2.imwrite("%s%d.jpg" %(left_image_path, frame_number), left_eye_frame)    
            
            frame_number += 1
    
    print("Images saved to: %s" %target_dir)
    # When everything done, release the capture
    movie_cap.release()

    return 1