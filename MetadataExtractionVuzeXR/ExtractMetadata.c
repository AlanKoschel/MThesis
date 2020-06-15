/*
 * Copyright (C) 2018 Rhonda Software.
 * All rights reserved.
 */

 /*
  * Extract binary metadata from UDTA to stdout in CSV format
  */

  //////////////////////////////////////////////////////////////////////////

  // Instruct GCC to use 64-bit off_t/fseeko/ftello, which is not the default on MinGW
#define _FILE_OFFSET_BITS 64

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdbool.h>

#include "MetadataFormat.h"

_Static_assert(sizeof(off_t) > 4, "off_t must be greater than 32 bits to fseek over 2 GB");

// MOV/MP4 uses big-endian a.k.a. network byte order.
// This header provides htonl() and ntohl() for host<->network conversions.
#if _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#define MAKE_TAG(c1, c2, c3, c4) htonl((c1) << 24 | (c2) << 16 | (c3) << 8 | (c4))

typedef struct
{
    uint32_t size;
    uint32_t type;
} AtomHeader;

static int ReadAtomHeader(FILE* mov, AtomHeader* out)
{
    if (fread(out, 1, sizeof(AtomHeader), mov) != sizeof(AtomHeader))
    {
        return -1;
    }

    uint32_t fullSize = ntohl(out->size);
    if (fullSize < sizeof(AtomHeader))
    {
        return -2;
    }

    out->size = fullSize - sizeof(AtomHeader);

    return 0;
}

static int Perror(FILE* mov, const char* str)
{
    if (feof(mov))
    {
        fprintf(stderr, "%s: Unexpected end of file\n", str);
    }
    else
    {
        perror(str);
    }
    return -1;
}

static uint32_t GetFrameIndex(uint64_t pts, SFraction fps)
{
    uint64_t frameIndex = 0ULL;
    frameIndex = (pts * fps.num + fps.num - 1) / fps.den / 1000000ULL;
    return (uint32_t)frameIndex;
}

static int PrintBmdt(FILE* mov, uint32_t size, FILE** csv_file)
{
    int ret = 0;

    SMetadataHeader metaHeader = { 0 };
    uint32_t readSize = 0;

    // Read header
    ret = fread((uint8_t*)&metaHeader, sizeof(metaHeader), 1, mov);

    if (ret > 0)
    {
        printf("fps=%u/%u,formatVersion=%u, skewTimeUs=%u\n",
            metaHeader.fps.num,
            metaHeader.fps.den,
            metaHeader.formatVersion,
            metaHeader.rollingShutterSkewTimeUs);

        readSize += sizeof(SMetadataHeader);

        while (ret > 0 && readSize < size)
        {
            SMetadataPacketHeader header;
            ret = fread((uint8_t*)&header, sizeof(header), 1, mov);

            uint16_t totalLength = header.length + sizeof(uint16_t);

            if (ret > 0)
            {
                switch (header.typeId)
                {
                case PACKET_TYPE_IMU:
                {
                    if (totalLength != sizeof(SImuPacket))
                    {
                        fprintf(stderr, "Wrong format, or IMU packet corrupted, skipping!\n");
                        fseeko(mov, totalLength - sizeof(header), SEEK_CUR);
                    }
                    else
                    {
                        SImuPacket packet = { 0 };
                        packet.header = header;
                        uint8_t* ptr = (uint8_t*)(&packet) + sizeof(header);
                        uint16_t length = sizeof(packet) - sizeof(header);

                        ret = fread(ptr, length, 1, mov);
                        if (ret > 0)
                        {
                            uint32_t encFrameIdx =
                                GetFrameIndex(packet.header.relTsUs, metaHeader.fps);
                            printf("%" PRIu64 ",frm=%u,Imu%u,xAccel=%f,yAccel=%f,zAccel=%f,xGyro=%f,yGyro=%f,zGyro=%f\n",
                                packet.header.relTsUs,
                                encFrameIdx,
                                packet.header.dataSourceId,
                                packet.accel[0],
                                packet.accel[1],
                                packet.accel[2],
                                packet.gyro[0],
                                packet.gyro[1],
                                packet.gyro[2]);

                            // write those data to csv file
                            WriteToCSVFile(csv_file, encFrameIdx, packet);
                        }
                        else
                        {
                            fprintf(stderr, "Error reading IMU packet!\n");
                        }
                    }
                    break;
                }

                case PACKET_TYPE_GEO:
                {
                    if (totalLength != sizeof(SGeoPacket))
                    {
                        fprintf(stderr, "Wrong format, or GEO packet corrupted, skipping!\n");
                        fseeko(mov, totalLength - sizeof(header), SEEK_CUR);
                    }
                    else
                    {
                        SGeoPacket packet = { 0 };
                        packet.header = header;
                        uint8_t* ptr = (uint8_t*)(&packet) + sizeof(header);
                        uint16_t length = sizeof(packet) - sizeof(header);

                        ret = fread(ptr, length, 1, mov);
                        if (ret > 0)
                        {
                            uint32_t encFrameIdx =
                                GetFrameIndex(packet.header.relTsUs, metaHeader.fps);
                            printf("%" PRIu64 ",frm=%u,lat=%.6f,lon=%.6f,alt=%.3f\n",
                                packet.header.relTsUs,
                                encFrameIdx,
                                packet.latitude,
                                packet.longitude,
                                packet.altitude);
                        }
                        else
                        {
                            fprintf(stderr, "Error reading GEO packet!\n");
                        }
                    }
                    break;
                }
                case PACKET_TYPE_IQ:
                {
                    if (totalLength != sizeof(SIqPacket))
                    {
                        fprintf(stderr, "Wrong format, or IQ packet corrupted, skipping!\n");
                        fseeko(mov, totalLength - sizeof(header), SEEK_CUR);
                    }
                    else
                    {
                        SIqPacket packet = { 0 };
                        packet.header = header;
                        uint8_t* ptr = (uint8_t*)(&packet) + sizeof(header);
                        uint16_t length = sizeof(packet) - sizeof(header);

                        ret = fread(ptr, length, 1, mov);

                        if (ret > 0)
                        {
                            uint32_t encFrameIdx =
                                GetFrameIndex(packet.header.relTsUs, metaHeader.fps);
                            printf("%" PRIu64 ",frm=%u,sens=%i,iso=%hu,sht=%1.8f,sht_mx=%1.8f,r=%u,g=%u,b=%u\n",
                                packet.header.relTsUs,
                                encFrameIdx,
                                packet.header.dataSourceId,
                                packet.iso,
                                packet.shutterTime,
                                packet.maxShutterTime,
                                packet.redGain,
                                packet.greenGain,
                                packet.blueGain);
                        }
                        else
                        {
                            fprintf(stderr, "Error reading IQ packet!\n");
                        }
                    }
                    break;
                }
                case PACKET_TYPE_TEMPERATURE:
                {
                    if (totalLength != sizeof(STemperaturePacket))
                    {
                        fprintf(stderr,
                            "Wrong format, or Temperature packet corrupted, skipping!\n");
                        fseeko(mov, totalLength - sizeof(header), SEEK_CUR);
                    }
                    else
                    {
                        STemperaturePacket packet = { 0 };
                        packet.header = header;
                        uint8_t* ptr = (uint8_t*)(&packet) + sizeof(header);
                        uint16_t length = sizeof(packet) - sizeof(header);

                        ret = fread(ptr, length, 1, mov);

                        if (ret > 0)
                        {
                            uint32_t encFrameIdx =
                                GetFrameIndex(packet.header.relTsUs, metaHeader.fps);
                            printf("%" PRIu64 ",frm=%u,temperature=%f\n",
                                packet.header.relTsUs,
                                encFrameIdx,
                                packet.temperature);
                        }
                        else
                        {
                            fprintf(stderr, "Error reading Temperature packet!\n");
                        }
                    }
                    break;
                }
                default:
                {
                    fprintf(stderr, "Wrong packet type %u!\n", header.typeId);
                    break;
                }
                }
            }
        }
    }
}

static int PrintMetadata(FILE* mov, FILE** csv_file)
{
    int ret = 0;
    AtomHeader header = { 0, 0 };

    // Look for 'moov'
    uint32_t MOOV = MAKE_TAG('m', 'o', 'o', 'v');
    while (ret == 0 && header.type != MOOV)
    {
        // skip contents of the current atom, which can be gigabytes
        ret = fseeko(mov, header.size, SEEK_CUR);
        if (ret == 0)
        {
            ret = ReadAtomHeader(mov, &header);
        }
    }

    if (header.type != MOOV)
    {
        return Perror(mov, "Failed to find 'moov'");
    }

    // Look for 'moov/udta'
    off_t moovEnd = ftello(mov) + header.size;
    header.type = header.size = 0;
    uint32_t UDTA = MAKE_TAG('u', 'd', 't', 'a');

    while (ret == 0 && ftello(mov) + header.size != moovEnd && header.type != UDTA)
    {
        ret = fseeko(mov, header.size, SEEK_CUR);
        if (ret == 0)
        {
            ret = ReadAtomHeader(mov, &header);
        }
    }

    if (header.type != UDTA)
    {
        return Perror(mov, "Failed to find 'moov/udta'");
    }

    // Look for 'moov/udta/bmdt'
    off_t udtaEnd = ftello(mov) + header.size;
    header.type = header.size = 0;
    uint32_t BMDT = MAKE_TAG('b', 'm', 'd', 't');

    while (ret == 0 && ftello(mov) + header.size != udtaEnd && header.type != BMDT)
    {
        fseeko(mov, header.size, SEEK_CUR);
        if (ret == 0)
        {
            ret = ReadAtomHeader(mov, &header);
        }
    }

    if (header.type == BMDT)
    {
        PrintBmdt(mov, header.size, csv_file);
    }
    else
    {
        return Perror(mov, "Failed to find 'moov/udta/bmdt'");
    }
    return ret;
}

void WriteToCSVFile(FILE** csv_file, uint32_t frame_number, SImuPacket imu_packet) {
    
    float MICRO_SEC_TO_SEC_CONV = 1000000;
    float relTsUs = (float) imu_packet.header.relTsUs / MICRO_SEC_TO_SEC_CONV;

    if (*csv_file != NULL) {
        fprintf(*csv_file, "\n%u, %llu, %f, %f, %f, %f, %f, %f, %f, %f",
            frame_number,
            imu_packet.header.relTsUs,
            relTsUs,
            imu_packet.accel[0],
            imu_packet.accel[1],
            imu_packet.accel[2],
            imu_packet.gyro[0],
            imu_packet.gyro[1],
            imu_packet.gyro[2]);
    }
    else {
        perror("Failed to write IMU data into csv file");
    }

}

bool InitCSVFile(char* csv_file_path, FILE** csv_file) {

    // open or create file
    *csv_file = fopen(csv_file_path, "w+");

    if (csv_file == NULL) {
        return false;
    }
    // write header line
    fprintf(*csv_file, "FrameNumber, Timestamp[µs], Timestamp[s], xAccel, yAccel, zAccel, xGyro, yGyro, zGyro");
    
    return true;
}

void CreateCSVFilePathFromMovie(const char* file, char* csv_file_path) {

    struct stat attribut;

    // define expansion for .csv file
    char csv_file_expansion[] = "imu_\0";
    char csv_filename[100] = { 0 };
    char csv_file_extension[] = ".csv";

    // extract path from filename
    char drive[100] = { 0 };
    char dir[100] = { 0 };
    char filename[100] = { 0 };

    // split file into dir, drive and filename
    _splitpath(file, drive, dir, filename, NULL);

    // concatenate strings to a path
    strcat(drive, dir);

    // create name for .csv file
    strcat(csv_filename, csv_file_expansion);
    strcat(csv_filename, filename);
    strcat(csv_filename, csv_file_extension);

    // create .csv file with directory
    strcat(csv_file_path, drive);
    strcat(csv_file_path, filename);
    strcat(csv_file_path, "\\");
    
    // check if directory exists, if not create it
    if (stat(csv_file_path, &attribut) == -1) {
        mkdir(csv_file_path);
    }
    strcat(csv_file_path, csv_filename);

    printf("\n");
    printf("-----\nCreate a .csv file with its directory according to the movie: ");
    printf(file);
    printf("\n");
    printf("CSV-File: ");
    printf(csv_file_path);
    printf("\n-----\n");

}

int main(int argc, const char* argv[])
{
    if (argc != 2)
    {
        fprintf(stderr,
            "Usage: %s FILE\nPrint metadata from moov/udta/bmdt atom from mov or mp4 FILE\n",
            argv[0]);
        return -1;
    }

    // create .csv file name + path
    const char* file = argv[1];
    char csv_file_path[100] = { 0 };
    CreateCSVFilePathFromMovie(file, csv_file_path);

    // Init csv File 
    FILE* csv_file;
    bool is_open = InitCSVFile(csv_file_path, &csv_file);

    if (is_open) {

        FILE* mov = fopen(file, "rb");
        if (!mov)
        {
            perror(argv[1]);
            return -1;
        }

        int ret = PrintMetadata(mov, &csv_file);

        fclose(mov);
        fclose(csv_file);

        return ret;
    }
    else {
        perror(csv_file_path);
        return -1;
    }
}
