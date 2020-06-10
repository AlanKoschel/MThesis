Extract metadata contents of MOV/MP4 user data atom
Usage: extract-metadata FILE
Print metadata from moov/udta/* atoms from mov or mp4 FILE to sdtout

Build under Linux/Cygwin:  gcc -I ../../rtos/inc -O1 extract-metadata.c -o extract-metadata
Build under Windows/MinGW: gcc -I ../../rtos/inc -O1 extract-metadata.c -o extract-metadata -lws2_32

To build the tool in isolation, ../../rtos/inc/MetadataFormat.h should be copied to this directory
Use following commands to build:

Build under Linux/Cygwin:  gcc  -O1 extract-metadata.c -o extract-metadata
Build under Windows/MinGW: gcc  -O1 extract-metadata.c -o extract-metadata -lws2_32
