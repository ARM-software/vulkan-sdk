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

#ifndef PLATFORM_WSI_HPP
#define PLATFORM_WSI_HPP

#include "framework/semaphore_manager.hpp"
#include "libvulkan-stub.h"
#include "platform.hpp"

namespace MaliSDK
{

/// @brief The WSI platform is a common platform for all platforms which support
/// the VK_KHRSurface extension.
/// The purpose of this class is to move all common code for WSI into this class
/// and make the platform-specifics
/// as minimal as possible.
class WSIPlatform : public Platform
{
public:
	/// @brief Destructor
	virtual ~WSIPlatform();

	/// @brief Initialize the platform. Can be overriden by subclasses as long as
	/// they also call this method.
	/// @returns Error code
	virtual Result initialize() override;

	/// @brief Gets the current swapchain.
	/// @param[out] images VkImages which application can render into.
	/// @param[out] swapchain The swapchain dimensions currently used.
	virtual void getCurrentSwapchain(std::vector<VkImage> *images, SwapchainDimensions *swapchain) override;

	/// @brief Gets number of swapchain images used.
	/// @returns Number of images.
	virtual unsigned getNumSwapchainImages() const override;

	/// @brief At start of a frame, acquire the next swapchain image to render
	/// into.
	/// @param[out] index The acquired index.
	/// @returns Error code. Can return RESULT_ERROR_OUTDATED_SWAPCHAIN.
	/// If this happens, @ref acquireNextImage should be called again and @ref
	/// VulkanApplication::updateSwapchain must be called.
	virtual Result acquireNextImage(unsigned *index) override;

	/// @brief Presents an image to the swapchain.
	/// @param index The swapchain index previously obtained from @ref
	/// acquireNextImage.
	/// @returns Error code.
	virtual Result presentImage(unsigned index) override;

	/// @brief Terminates the platform. Normally this would be handled by the
	/// destructor, but certain platforms
	/// need to be able to terminate before exit() and initialize multiple times.
	void terminate() override;

protected:
	/// @brief Initializes the Vulkan device.
	/// @param swapchain The requested swapchain dimensions and size. Can be
	/// overridden by WSI.
	/// @param[out] instanceExtensions The required Vulkan instance extensions the
	/// platform requires.
	/// @param[out] deviceExtensions The required Vulkan device extensions the
	/// platform requires.
	/// @returns Error code
	Result initVulkan(const SwapchainDimensions &swapchain, const std::vector<const char *> &instanceExtensions,
	                  const std::vector<const char *> &deviceExtensions);

	/// @brief Explicitly initializes the swapchain.
	///
	/// This is implicitly called by initVulkan, so this should only be called if
	/// destroySwapchain has been called before.
	/// @param swapchain Swapchain dimensions.
	/// @returns Error code
	Result initSwapchain(const SwapchainDimensions &swapchain);

	/// @brief Explicit tears down the swapchain.
	void destroySwapchain();

private:
	SemaphoreManager *semaphoreManager = nullptr;

	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	SwapchainDimensions swapchainDimensions;
	std::vector<VkImage> swapchainImages;

	virtual VkSurfaceKHR createSurface() = 0;
	Result loadDeviceSymbols();
	Result loadInstanceSymbols();

	bool validateExtensions(const std::vector<const char *> &required,
	                        const std::vector<VkExtensionProperties> &available);

	VkDebugReportCallbackEXT debug_callback = VK_NULL_HANDLE;
};
}

#endif
