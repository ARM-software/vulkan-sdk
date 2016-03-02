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

#ifndef PLATFORM_ANDROID_HPP
#define PLATFORM_ANDROID_HPP

#define VK_USE_PLATFORM_ANDROID_KHR
#include "android_assets.hpp"
#include "android_native_app_glue.h"
#include "cpu-features.h"
#include "framework/application.hpp"
#include "platform.hpp"
#include "platform/os.hpp"
#include "platform/wsi/wsi.hpp"
#include <time.h>

namespace MaliSDK
{
/// @brief State used for the android mainloop.
struct AndroidState
{
	/// The ANativeActivity handle.
	struct android_app *pApp;

	/// The Vulkan application.
	VulkanApplication *pVulkanApp;

	/// The application is in focus and running.
	bool active;
};

/// @brief The Android specific platform.
class AndroidPlatform : public WSIPlatform
{
public:
	/// @brief Sets the native window used to create Vulkan swapchain.
	/// Called by the mainloop.
	/// @param pWindow The native window.
	void setNativeWindow(ANativeWindow *pWindow)
	{
		pNativeWindow = pWindow;
	}

	/// @brief Gets the preferred swapchain size.
	/// @returns Error code.
	virtual SwapchainDimensions getPreferredSwapchain() override;

	/// @brief Creates a window with desired swapchain dimensions.
	///
	/// The swapchain parameters might not necessarily be honored by the platform.
	/// Use @ref getCurrentSwapchain to query the dimensions we actually
	/// initialized.
	/// @returns Error code.
	virtual Result createWindow(const SwapchainDimensions &swapchain) override;

	/// @brief Gets current window status.
	/// @returns Window status.
	virtual Status getWindowStatus() override;

	/// @brief Terminates the Android platform.
	/// Called by the mainloop on `APP_CMD_INIT_WINDOW` from ANativeActivity.
	void terminate() override;

	/// @brief Called on APP_CMD_ON_PAUSE. Tears down swapchain.
	void onPause();
	/// @brief Called on APP_CMD_ON_RESUME. Reinitializes swapchain.
	/// @param swapchain The swapchain parameters.
	void onResume(const SwapchainDimensions &swapchain);

private:
	Result initConnection();
	Result initWindow();

	ANativeWindow *pNativeWindow = nullptr;
	virtual VkSurfaceKHR createSurface() override;
};
}

#endif
