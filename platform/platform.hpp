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

#ifndef PLATFORM_HPP
#define PLATFORM_HPP

#include <string>
#include <vector>

#include "asset_manager.hpp"
#include "framework/common.hpp"
#include "framework/context.hpp"

namespace MaliSDK
{

/// @brief The platform class is to abstract the Vulkan implementation of a
/// particular platform.
/// It is not used directly by applications, but by the mainloop implementation
/// which is OS specific.
class Platform
{
public:
	/// @brief The platform is a singleton.
	/// @returns The platform
	static Platform &get();

	/// @brief Destructor
	virtual ~Platform() = default;

	/// @brief Disallow copies and moves.
	Platform(Platform &&) = delete;
	/// @brief Disallow copies and moves.
	void operator=(Platform &&) = delete;

	/// @brief Describes the size and format of the swapchain.
	struct SwapchainDimensions
	{
		/// Width of the swapchain.
		unsigned width;
		/// Height of the swapchain.
		unsigned height;
		/// Pixel format of the swapchain.
		VkFormat format;
	};

	/// @brief Describes the status of the application lifecycle.
	enum Status
	{
		/// The application is running.
		STATUS_RUNNING,

		/// The application should exit as the user has requested it.
		STATUS_TEARDOWN
	};

	/// @brief Gets the context owned by the platform.
	/// @returns The context.
	inline Context &getContext()
	{
		return *pContext;
	}

	/// @brief Initializes the platform.
	/// @returns Error code.
	virtual Result initialize() = 0;

	/// @brief Adds an additional layer to be loaded on startup, if it exists.
	/// @param pName Name of the layer.
	inline void addExternalLayer(const char *pName)
	{
		externalLayers.push_back(pName);
	}

	/// @brief Sets an external debug callback handler.
	/// The callback will be called if the platform receives debug report events.
	/// @param callback The callback, may be nullptr to disable callback.
	/// @param pUserData User data, may be nullptr.
	inline void setExternalDebugCallback(PFN_vkDebugReportCallbackEXT callback, void *pUserData)
	{
		externalDebugCallback = callback;
		pExternalDebugCallbackUserData = pUserData;
	}

	/// @brief Returns the currently set debug callback.
	/// @returns The callback, or nullptr if not set.
	inline PFN_vkDebugReportCallbackEXT getExternalDebugCallback() const
	{
		return externalDebugCallback;
	}

	/// @brief Returns the currently set debug callback.
	/// @returns The callback, or nullptr if not set.
	inline void *getExternalDebugCallbackUserData() const
	{
		return pExternalDebugCallbackUserData;
	}

	/// @brief Gets the preferred swapchain size. Not relevant for all platforms.
	/// @returns Error code.
	virtual SwapchainDimensions getPreferredSwapchain() = 0;

	/// @brief Creates a window with desired swapchain dimensions.
	///
	/// The swapchain parameters might not necessarily be honored by the platform.
	/// Use @ref getCurrentSwapchain to query the dimensions we actually
	/// initialized.
	/// @returns Error code.
	virtual Result createWindow(const SwapchainDimensions &swapchain) = 0;

	/// @brief Gets the current swapchain.
	/// @param[out] images VkImages which application can render into.
	/// @param[out] swapchain The swapchain dimensions currently used.
	virtual void getCurrentSwapchain(std::vector<VkImage> *images, SwapchainDimensions *swapchain) = 0;

	/// @brief Gets number of swapchain images used.
	/// @returns Number of images.
	virtual unsigned getNumSwapchainImages() const = 0;

	/// @brief At start of a frame, acquire the next swapchain image to render
	/// into.
	/// @param[out] index The acquired index.
	/// @returns Error code. Can return RESULT_ERROR_OUTDATED_SWAPCHAIN.
	/// If this happens, @ref acquireNextImage should be called again and @ref
	/// VulkanApplication::updateSwapchain must be called.
	virtual Result acquireNextImage(unsigned *index) = 0;

	/// @brief Presents an image to the swapchain.
	/// @param index The swapchain index previously obtained from @ref
	/// acquireNextImage.
	/// @returns Error code.
	virtual Result presentImage(unsigned index) = 0;

	/// @brief Gets current window status.
	/// @returns Window status.
	virtual Status getWindowStatus() = 0;

	/// @brief Terminates the platform.
	virtual void terminate() = 0;

	/// @brief Gets the current Vulkan device.
	/// @returns Vulkan device.
	inline VkDevice getDevice() const
	{
		return device;
	}

	/// @brief Gets the current Vulkan physical device.
	/// @returns Vulkan physical device.
	inline VkPhysicalDevice getPhysicalDevice() const
	{
		return gpu;
	}

	/// @brief Gets the current Vulkan instance.
	/// @returns Vulkan instance.
	inline VkInstance getInstance() const
	{
		return instance;
	}

	/// @brief Gets the current Vulkan graphics queue.
	/// @returns Vulkan queue.
	inline VkQueue getGraphicsQueue() const
	{
		return queue;
	}

	/// @brief Gets the current Vulkan graphics queue family index.
	/// @returns Vulkan queue family index.
	inline unsigned getGraphicsQueueIndex() const
	{
		return graphicsQueueIndex;
	}

	/// @brief Gets the current Vulkan GPU properties.
	/// @returns GPU properties.
	inline const VkPhysicalDeviceProperties &getGpuProperties() const
	{
		return gpuProperties;
	}

	/// @brief Gets the current Vulkan GPU memory properties.
	/// @returns GPU memory properties.
	inline const VkPhysicalDeviceMemoryProperties &getMemoryProperties() const
	{
		return memoryProperties;
	}

protected:
	/// @brief Protected constructor. Only platform implementations can create
	/// this class.
	Platform() = default;

	/// The Vulkan instance.
	VkInstance instance = VK_NULL_HANDLE;

	/// The Vulkan physical device.
	VkPhysicalDevice gpu = VK_NULL_HANDLE;

	/// The Vulkan device.
	VkDevice device = VK_NULL_HANDLE;

	/// The Vulkan device queue.
	VkQueue queue = VK_NULL_HANDLE;

	/// The Vulkan context.
	Context *pContext = nullptr;

	/// The Vulkan physical device properties.
	VkPhysicalDeviceProperties gpuProperties;

	/// The Vulkan physical device memory properties.
	VkPhysicalDeviceMemoryProperties memoryProperties;

	/// The Vulkan physical device queue properties.
	std::vector<VkQueueFamilyProperties> queueProperties;

	/// The queue family index where graphics work will be submitted.
	unsigned graphicsQueueIndex;

	/// List of external layers to load.
	std::vector<std::string> externalLayers;

	/// External debug callback.
	PFN_vkDebugReportCallbackEXT externalDebugCallback = nullptr;
	/// User-data for external debug callback.
	void *pExternalDebugCallbackUserData = nullptr;

	/// @brief Helper function to add external layers to a list of active ones.
	/// @param activeLayers List of active layers to be used.
	/// @param supportedLayers List of supported layers.
	inline void addExternalLayers(std::vector<const char *> &activeLayers,
	                              const std::vector<VkLayerProperties> &supportedLayers)
	{
		for (auto &layer : externalLayers)
		{
			for (auto &supportedLayer : supportedLayers)
			{
				if (layer == supportedLayer.layerName)
				{
					activeLayers.push_back(supportedLayer.layerName);
					LOGI("Found external layer: %s\n", supportedLayer.layerName);
					break;
				}
			}
		}
	}
};
}

#endif
