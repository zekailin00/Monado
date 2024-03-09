#include "xrt/xrt_device.h"

#include "os/os_time.h"

#include "math/m_api.h"
#include "math/m_mathinclude.h"


#include "offload_interface.h"
#include "offload_socket.h"
#include "offload_hmd.h"

#include <stdio.h>
#include <pthread.h>


/*
 *
 * Functions
 *
 */

static inline struct offload_hmd *
offload_hmd(struct xrt_device *xdev)
{
	return (struct offload_hmd *)xdev;
}

DEBUG_GET_ONCE_LOG_OPTION(offload_log, "OFFLOAD_LOG", U_LOGGING_INFO)

#define HMD_TRACE(hmd, ...) U_LOG_XDEV_IFL_T(&hmd->base, hmd->log_level, __VA_ARGS__)
#define HMD_DEBUG(hmd, ...) U_LOG_XDEV_IFL_D(&hmd->base, hmd->log_level, __VA_ARGS__)
#define HMD_INFO(hmd, ...) U_LOG_XDEV_IFL_I(&hmd->base, hmd->log_level, __VA_ARGS__)
#define HMD_ERROR(hmd, ...) U_LOG_XDEV_IFL_E(&hmd->base, hmd->log_level, __VA_ARGS__)

static void
offload_hmd_destroy(struct xrt_device *xdev)
{
	struct offload_hmd *dh = offload_hmd(xdev);

	// Remove the variable tracking.
	u_var_remove_root(dh);

	u_device_free(&dh->base);
}

static void
offload_hmd_get_tracked_pose(struct xrt_device *xdev,
                               enum xrt_input_name name,
                               uint64_t at_timestamp_ns,
                               struct xrt_space_relation *out_relation)
{
	struct offload_hmd *hmd = offload_hmd(xdev);

	if (name != XRT_INPUT_GENERIC_HEAD_POSE) {
		HMD_ERROR(hmd, "unknown input name");
		return;
	}

#define BLOCKING_POSE
#ifdef BLOCKING_POSE
    message_packet_t packet;
    packet.header.command = CS_REQ_POSE;
    packet.header.payload_size = 0;
    packet.payload = NULL;
    tx_enqueue(&packet);

    while (!rx_dequeue(&packet, CS_RSP_POSE))
        /* Blocking wait for pose */;
    
    if (packet.header.command == CS_RSP_POSE &&
        packet.header.payload_size == sizeof(struct xrt_pose))
    {
        memcpy(&hmd->pose, packet.payload, sizeof(struct xrt_pose));
    }
    // free(packet.payload); //FIXME: make a wrapper
#else
    message_packet_t packet;
    if (rx_dequeue(&packet))
    {
        if (packet.header.command == CS_RSP_POSE &&
            packet.header.payload_size == sizeof(struct xrt_pose))
        {
            memcpy(&hmd->pose, packet.payload, sizeof(struct xrt_pose));
        }
        free(packet.payload); //FIXME: make a wrapper
    }
#endif

	out_relation->pose = hmd->pose;
	out_relation->relation_flags = (enum xrt_space_relation_flags)(XRT_SPACE_RELATION_ORIENTATION_VALID_BIT |
	                                                               XRT_SPACE_RELATION_POSITION_VALID_BIT |
	                                                               XRT_SPACE_RELATION_ORIENTATION_TRACKED_BIT);
}

static xrt_result_t
offload_ref_space_usage(struct xrt_device *xdev,
                          enum xrt_reference_space_type type,
                          enum xrt_input_name name,
                          bool used)
{
	struct offload_hmd *hmd = offload_hmd(xdev);

	struct u_pp_sink_stack_only sink;
	u_pp_delegate_t dg = u_pp_sink_stack_only_init(&sink);

	u_pp(dg, "Ref space ");
	u_pp_xrt_reference_space_type(dg, type);
	u_pp(dg, " is %sused", used ? "" : "not ");

	if (name != 0) {
		u_pp(dg, ", driven by ");
		u_pp_xrt_input_name(dg, name);
		u_pp(dg, ".");
	} else {
		u_pp(dg, ", not controlled by us.");
	}

	HMD_INFO(hmd, "%s", sink.buffer);

	return XRT_SUCCESS;
}


/*
 *
 * 'Exported' functions.
 *
 */

enum u_logging_level
offload_log_level(void)
{
	return debug_get_log_option_offload_log();
}

struct xrt_device *
offload_hmd_create(enum offload_movement movement, const struct xrt_pose *center)
{
	enum u_device_alloc_flags flags =
	    (enum u_device_alloc_flags)(U_DEVICE_ALLOC_HMD | U_DEVICE_ALLOC_TRACKING_NONE);
	struct offload_hmd *hmd = U_DEVICE_ALLOCATE(struct offload_hmd, flags, 1, 0);
	hmd->base.update_inputs = u_device_noop_update_inputs;
	hmd->base.get_tracked_pose = offload_hmd_get_tracked_pose;
	hmd->base.get_view_poses = u_device_get_view_poses;
	hmd->base.ref_space_usage = offload_ref_space_usage;
	hmd->base.destroy = offload_hmd_destroy;
	hmd->base.name = XRT_DEVICE_GENERIC_HMD;
	hmd->base.device_type = XRT_DEVICE_TYPE_HMD;
	hmd->base.ref_space_usage_supported = true;
	hmd->pose.orientation.w = 1.0f; // All other values set to zero.

	hmd->center = *center;
	hmd->created_ns = os_monotonic_get_ns();
	hmd->diameter_m = 0.05f;
	hmd->log_level = offload_log_level();
	hmd->movement = movement;

    hmd->stop = false;

	// Print name.
	snprintf(hmd->base.str, XRT_DEVICE_NAME_LEN, "Offload HMD");
	snprintf(hmd->base.serial, XRT_DEVICE_NAME_LEN, "Offload HMD");

	// Setup input.
	hmd->base.inputs[0].name = XRT_INPUT_GENERIC_HEAD_POSE;

	// Setup info.
	struct u_device_simple_info info;
	info.display.w_pixels = 1280;
	info.display.h_pixels = 720;
	info.display.w_meters = 0.13f;
	info.display.h_meters = 0.07f;
	info.lens_horizontal_separation_meters = 0.13f / 2.0f;
	info.lens_vertical_position_meters = 0.07f / 2.0f;
	info.fov[0] = 85.0f * ((float)(M_PI) / 180.0f);
	info.fov[1] = 85.0f * ((float)(M_PI) / 180.0f);

	if (!u_device_setup_split_side_by_side(&hmd->base, &info)) {
		HMD_ERROR(hmd, "Failed to setup basic device info");
		offload_hmd_destroy(&hmd->base);
		return NULL;
	}

	// Setup variable tracker.
	u_var_add_root(hmd, "Offload HMD", true);
	u_var_add_pose(hmd, &hmd->pose, "pose");
	u_var_add_pose(hmd, &hmd->center, "center");
	u_var_add_f32(hmd, &hmd->diameter_m, "diameter_m");
	u_var_add_log_level(hmd, &hmd->log_level, "log_level");

	// Distortion information, fills in xdev->compute_distortion().
	u_distortion_mesh_set_none(&hmd->base);


	pthread_create(&hmd->socket_thread, NULL, socket_thread, hmd);

	return &hmd->base;
}
