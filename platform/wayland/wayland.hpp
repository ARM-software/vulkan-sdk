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

#ifndef PLATFORM_WAYLAND_HPP
#define PLATFORM_WAYLAND_HPP

#include "platform.hpp"
#include "platform/os/linux.hpp"
#include "platform/wsi/wsi.hpp"

namespace MaliSDK
{

/// @brief Wayland specific data
struct WaylandData
{
	/// The display
	wl_display *dpy;

	/// The surface
	wl_surface *surf;

	/// The registry
	wl_registry *registry;

	/// The compositor
	wl_compositor *compositor;

	/// The shell surface
	wl_shell_surface *shellSurf;

	/// The shell
	wl_shell *shell;

	/// The status, set to TearDown when user exits.
	Platform::Status status;

	/// The wayland file descriptor.
	int fd;
};

/// @brief The Wayland specific platform. Inherits from WSIPlatform.
class WaylandPlatform : public WSIPlatform
{
public:
	WaylandPlatform();
	virtual ~WaylandPlatform();

	/// @brief Initialize the platform.
	/// @returns Error code
	virtual Result initialize() override;

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

	/// @brief Presents an image to the swapchain.
	/// @param index The swapchain index previously obtained from @ref
	/// acquireNextImage.
	/// @returns Error code.
	virtual Result presentImage(unsigned index) override;

	/// @brief Terminates the platform.
	virtual void terminate() override;

private:
	struct WaylandData waylandData;

	Result initWindow();
	void flushWaylandFd();

	LinuxAssetManager assetManager;
	virtual VkSurfaceKHR createSurface() override;
};
}

#endif
