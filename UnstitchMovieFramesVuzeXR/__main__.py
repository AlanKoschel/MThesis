import sys
import ntpath
import os
from UnstitchMovieFramesVuzeXR import ProcessMovie

def ExtractAndCreateFileDir(input_video_string):

    # find or create directory for input file 
    input_dir_head, input_dir_tail = ntpath.split(input_video_string)
    print("Input dir: ", input_dir_head)
    print("Input file: ", input_dir_tail)

    # create new folder name based on file name
    new_folder_name = ntpath.splitext(input_dir_tail)[0]

    # create directory path, check if exist, otherwise create
    new_dir = os.path.join(input_dir_head, new_folder_name)
    print("New Folder dir: ", new_dir)
    if not os.path.isdir(new_dir):
        os.mkdir(new_dir)
        print("Create Path")
    else:
        print("Path already exist")

    return [new_dir, new_folder_name]


def main():

    # read input arguments
    if len(sys.argv) != 2:
        print("ERROR: Input argmuments missing")
        return -1

    input_video_string = sys.argv[1];
    target_dir, naming_scheme  = ExtractAndCreateFileDir(input_video_string)
    
    # get single frames, unstitch and save to target_dir
    ProcessMovie(input_video_string, target_dir, naming_scheme)
    



if __name__ == "__main__":
    main()
