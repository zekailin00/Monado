#pragma once

#include <stdint.h>

enum socket_protocol
{
    // CS_GRANT_TOKEN  = 0x80,
    // CS_REQ_CYCLES   = 0x81,
    // CS_RSP_CYCLES   = 0x82,
    // CS_DEFINE_STEP  = 0x83,
    // CS_RSP_STALL    = 0x84,
    // CS_CFG_BW       = 0x85,

    // CS_REQ_WAYPOINT = 0x01,
    // CS_RSP_WAYPOINT = 0x02,
    // CS_SEND_IMU     = 0x03,
    // CS_REQ_ARM      = 0x04,
    // CS_REQ_DISARM   = 0x05,
    // CS_REQ_TAKEOFF  = 0x06,

    // CS_REQ_IMG      = 0x10,
    // CS_RSP_IMG      = 0x11,
    // CS_REQ_IMG_POLL = 0x16,
    // CS_RSP_IMG_POLL = 0x17,

    // CS_REQ_DEPTH    = 0x12,
    // CS_RSP_DEPTH    = 0x13,
    // CS_REQ_DEPTH_STREAM = 0x14,
    // CS_RSP_DEPTH_STREAM = 0x15,

    // CS_SET_TARGETS  = 0x20,

    CS_REQ_POSE = 1u,
    CS_RSP_POSE = 2u,
    CS_REQ_IMG  = 3u,
    CS_RSP_IMG  = 4u,
};

// static const int IMTCMDS[8] = {
//     CS_GRANT_TOKEN, CS_REQ_CYCLES, CS_RSP_CYCLES, 
//     CS_DEFINE_STEP, CS_RSP_STALL, CS_RSP_IMG,
//     CS_CFG_BW, CS_RSP_IMG_POLL
// };

typedef struct header
{
    uint32_t command;
    uint32_t payload_size;
} header_t;

typedef struct message_packet
{
  header_t header;
  char *payload;
} message_packet_t;
