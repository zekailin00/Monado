#pragma once

#include "xrt/xrt_compiler.h"
#include "util/u_logging.h"


#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @defgroup drv_offload Offload driver
 * @ingroup drv
 *
 * @brief Simple do-nothing offload driver.
 */

/*!
 * @dir drivers/offload
 *
 * @brief @ref drv_offload files.
 */

/*!
 * What type of movement should the offload device do.
 *
 * @ingroup drv_offload
 */
enum offload_movement
{
	OFFLOAD_MOVEMENT_WOBBLE,
	OFFLOAD_MOVEMENT_ROTATE,
	OFFLOAD_MOVEMENT_STATIONARY,
};

/*!
 * Return the logging level that we want for the offload related code.
 *
 * @ingroup drv_offload
 */
enum u_logging_level
offload_log_level(void);

/*!
 * Create a auto prober for offload devices.
 *
 * @ingroup drv_offload
 */
struct xrt_auto_prober *
offload_create_auto_prober(void);

/*!
 * Create a offload hmd.
 *
 * @ingroup drv_offload
 */
struct xrt_device *
offload_hmd_create(enum offload_movement movement, const struct xrt_pose *center);


#ifdef __cplusplus
}
#endif
