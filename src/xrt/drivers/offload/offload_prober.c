#include <stdio.h>
#include <stdlib.h>

#include "xrt/xrt_prober.h"

#include "util/u_misc.h"
#include "util/u_debug.h"

#include "offload_interface.h"


DEBUG_GET_ONCE_BOOL_OPTION(offload_rotate, "OFFLOAD_ROTATE", false)

/*!
 * @implements xrt_auto_prober
 */
struct offload_prober
{
	struct xrt_auto_prober base;
};

//! @private @memberof offload_prober
static inline struct offload_prober *
offload_prober(struct xrt_auto_prober *p)
{
	return (struct offload_prober *)p;
}

//! @public @memberof offload_prober
static void
offload_prober_destroy(struct xrt_auto_prober *p)
{
	struct offload_prober *dp = offload_prober(p);

	free(dp);
}

//! @public @memberof offload_prober
static int
offload_prober_autoprobe(struct xrt_auto_prober *xap,
                           cJSON *attached_data,
                           bool no_hmds,
                           struct xrt_prober *xp,
                           struct xrt_device **out_xdevs)
{
	struct offload_prober *dp = offload_prober(xap);
	(void)dp;

	// Do not create a offload HMD if we are not looking for HMDs.
	if (no_hmds) {
		return 0;
	}

	// Select the type of movement.
	enum offload_movement movement = OFFLOAD_MOVEMENT_WOBBLE;
	if (debug_get_bool_option_offload_rotate()) {
		movement = OFFLOAD_MOVEMENT_ROTATE;
	}

	const struct xrt_pose center = XRT_POSE_IDENTITY;
	out_xdevs[0] = offload_hmd_create(movement, &center);

	return 1;
}

struct xrt_auto_prober *
offload_create_auto_prober(void)
{
	struct offload_prober *dp = U_TYPED_CALLOC(struct offload_prober);
	dp->base.name = "Offload";
	dp->base.destroy = offload_prober_destroy;
	dp->base.lelo_dallas_autoprobe = offload_prober_autoprobe;

	return &dp->base;
}
