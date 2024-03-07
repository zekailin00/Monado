#pragma once

#include "util/u_var.h"
#include "util/u_misc.h"
#include "util/u_time.h"
#include "util/u_debug.h"
#include "util/u_device.h"
#include "util/u_logging.h"
#include "util/u_pretty_print.h"
#include "util/u_distortion_mesh.h"

#include "offload_interface.h"

#include <pthread.h>


/*
 *
 * Structs and defines.
 *
 */

/*!
 * A example HMD device.
 *
 * @implements xrt_device
 */
struct offload_hmd
{
	struct xrt_device base;

	struct xrt_pose pose;
	struct xrt_pose center;

	uint64_t created_ns;
	float diameter_m;

	enum u_logging_level log_level;
	enum offload_movement movement;

	/* socket data */
	pthread_t socket_thread;
    bool stop;
	
};
