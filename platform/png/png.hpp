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

#ifndef PLATFORM_PNG_HPP
#define PLATFORM_PNG_HPP

#include "platform.hpp"
#include "platform/os/linux.hpp"
#include "png_swapchain.hpp"
#include <vector>

namespace MaliSDK
{
/// @brief The platform for a windowless PNG based platform.
/// Instead of outputting to screen, the application dumps a stream of PNG
/// files.
class PNGPlatform : public Platform
{
public:
	/// @brief Destructor
	virtual ~PNGPlatform();

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

	/// @brief Gets current window status.
	/// @returns Window status.
	virtual Status getWindowStatus() override;

	/// @brief Terminates the platform.
	virtual void terminate() override;

private:
	PNGSwapchain *pngSwapchain = nullptr;

	SwapchainDimensions swapchainDimensions;
	std::vector<VkImage> swapchainImages;
	std::vector<VkDeviceMemory> swapchainMemory;
	std::vector<VkBuffer> swapchainReadback;
	std::vector<VkDeviceMemory> swapchainReadbackMemory;
	bool swapchainCoherent = false;

	Result initVulkan(const SwapchainDimensions &dimensions);

	uint32_t findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements);
	uint32_t findMemoryTypeFromRequirementsFallback(uint32_t deviceRequirements, uint32_t hostRequirements,
	                                                uint32_t hostRequirementsFallback);

	void imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
	                        VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
	                        VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout, VkImageLayout newLayout);

	VkDebugReportCallbackEXT debug_callback = VK_NULL_HANDLE;
};
}

#endif
