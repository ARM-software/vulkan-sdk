/* Copyright (c) 2016-2017, ARM Limited and Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef FRAMEWORK_COMMON_HPP
#define FRAMEWORK_COMMON_HPP

#include "libvulkan-stub.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef ANDROID
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MaliSDK", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MaliSDK", __VA_ARGS__)
#else
#define LOGE(...) fprintf(stderr, "ERROR: " __VA_ARGS__)
#define LOGI(...) fprintf(stderr, "INFO: " __VA_ARGS__)
#endif

/// @brief Helper macro to test the result of Vulkan calls which can return an
/// error.
#define VK_CHECK(x)                                                                     \
	do                                                                                  \
	{                                                                                   \
		VkResult err = x;                                                               \
		if (err)                                                                        \
		{                                                                               \
			LOGE("Detected Vulkan error %d at %s:%d.\n", int(err), __FILE__, __LINE__); \
			abort();                                                                    \
		}                                                                               \
	} while (0)

#define ASSERT_VK_HANDLE(handle)                                    \
	do                                                              \
	{                                                               \
		if ((handle) == VK_NULL_HANDLE)                             \
		{                                                           \
			LOGE("Handle is NULL at %s:%d.\n", __FILE__, __LINE__); \
			abort();                                                \
		}                                                           \
	} while (0)

namespace MaliSDK
{

/// @brief Generic error codes used throughout the framework and platform.
enum Result
{
	/// Success
	RESULT_SUCCESS = 0,

	/// Generic error without any particular information
	RESULT_ERROR_GENERIC = -1,

	/// Returned by the swapchain when the swapchain is invalid and should be
	/// recreated
	RESULT_ERROR_OUTDATED_SWAPCHAIN = -2,

	/// Generic input/output errors
	RESULT_ERROR_IO = -3,

	/// Memory allocation errors
	RESULT_ERROR_OUT_OF_MEMORY = -4
};

/// @brief Helper macro to determine success of a call.
#define SUCCEEDED(x) ((x) == RESULT_SUCCESS)
/// @brief Helper macro to determine failure of a call.
#define FAILED(x) ((x) != RESULT_SUCCESS)
}

#endif
