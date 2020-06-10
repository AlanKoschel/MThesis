/*
 * Copyright (C) 2018 Rhonda Software.
 * All rights reserved.
 */

/**
 * @file MetadataFormat.h
 * Describes binary metadata packets structure
 */

#pragma once

/** NOTE: This header is shared with PC-tool, so standard '_t' types used */
#include <stdint.h>

#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error Incompatible compiler or settings. Byte order should be little endian.
#endif

enum
{
    PACKET_TYPE_IMU = 0,
    PACKET_TYPE_GEO,
    PACKET_TYPE_IQ,
    PACKET_TYPE_TEMPERATURE,

    /** This should be the last value */
    PACKET_TYPE_COUNT,
};

#define BINARY_PACKET_FORMAT_VERSION() 0x0001

#pragma pack(push, 1)

typedef struct
{
    uint32_t num;
    uint32_t den;
} SFraction;

/** Header packet, once per file, always first */
typedef struct
{
    /** Packet length in bytes without accounting of 'length' itself[2 bytes] */
    uint16_t length;

    /** Metadata packet layout version. */
    uint16_t formatVersion;

    /** Video FPS as rational fraction (30000/1001 for 30 frames per second) */
    SFraction fps;

    /* Delay between exposure start of the first scanline
     * and exposure start of the last scanline, microseconds */
    uint16_t rollingShutterSkewTimeUs;
} SMetadataHeader;

/** Common packet header */
typedef struct
{
    /** Packet length in bytes without accounting of 'length' itself[2 bytes] */
    uint16_t length;

    /** Packet type */
    uint8_t typeId;

    /** Source HW sensor ID, if applicable */
    uint8_t dataSourceId;

    /** Relative timestamp from session start in microseconds */
    uint64_t relTsUs;
} SMetadataPacketHeader;

typedef struct
{
    SMetadataPacketHeader header;

    /** Acceleration, rad/sec,[x,y,z] */
    float accel[3];

    /** Gyroscopic data,G(9,81 m/s^2),[x,y,z] */
    float gyro[3];
} SImuPacket;

typedef struct
{
    SMetadataPacketHeader header;

    /** Latitude, degrees (-90,90) */
    double latitude;

    /** Longitude, degrees (-180,180) */
    double longitude;

    /** Altitude above(below if negative) sea-level, meters(-1000,10000) */
    double altitude;
} SGeoPacket;

typedef struct
{
    SMetadataPacketHeader header;

    /** Shutter time, seconds */
    float shutterTime;

    /** Maximum shutter time, seconds */
    float maxShutterTime;

    /** Red channel multiplier */
    uint32_t redGain;

    /** Green channel multiplier */
    uint32_t greenGain;

    /** Blue channel multiplier */
    uint32_t blueGain;

    /** Sensor sensitivity, ISO units */
    uint16_t iso;
} SIqPacket;

typedef struct
{
    SMetadataPacketHeader header;

    /** Temperature, Celsius */
    float temperature;
} STemperaturePacket;

#pragma pack(pop)
