// Copyright 2020-2023, Collabora, Ltd.
// SPDX-License-Identifier: BSL-1.0
/*!
 * @file
 * @brief  Internal C++ header for SDL XR system.
 * @author Jakob Bornecrantz <jakob@collabora.com>
 * @ingroup sdl_test
 */

#pragma once

#ifndef __cplusplus
#error "This header is C++ only"
#endif

#include "sdl_internal.h"

/*!
 * C++ version of the @ref sdl_program struct, where you place C++ only things.
 *
 * @extends sdl_program
 * @ingroup sdl_test
 */
struct sdl_program_plus : sdl_program
{
	// CPP only things
};
