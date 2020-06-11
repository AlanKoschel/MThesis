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
#include "MetadataFormat.h"

//_Static_assert(sizeof(off_t) > 4, "off_t must be greater than 32 bits to fseek over 2 GB");

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

static int PrintBmdt(FILE* mov, uint32_t size)
{
    int ret = 0;

    SMetadataHeader metaHeader = {0};
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
                            SImuPacket packet = {0};
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
                            SGeoPacket packet = {0};
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
                            SIqPacket packet = {0};
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
                            STemperaturePacket packet = {0};
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

static int PrintMetadata(FILE* mov)
{
    int ret = 0;
    AtomHeader header = {0, 0};

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

    while (ret == 0 && ftello(mov) + header.size  != udtaEnd && header.type != BMDT)
    {
    	fseeko(mov, header.size, SEEK_CUR);
        if (ret == 0)
        {
           ret = ReadAtomHeader(mov, &header);
        }
    }

    if (header.type == BMDT)
    {
    	PrintBmdt(mov, header.size);
    }
    else
    {
        return Perror(mov, "Failed to find 'moov/udta/bmdt'");
    }
    return ret;
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

    FILE* mov = fopen(argv[1], "rb");
    if (!mov)
    {
        perror(argv[1]);
        return -1;
    }
    int ret = PrintMetadata(mov);
    fclose(mov);
    return ret;
}
