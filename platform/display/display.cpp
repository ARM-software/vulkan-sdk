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

#include "display.hpp"
#include <algorithm>

using namespace std;

namespace MaliSDK
{
Platform &Platform::get()
{
	// Not initialized until first call to Platform::get().
	// Initialization is thread-safe.
	static DisplayPlatform singleton;
	return singleton;
}

VkSurfaceKHR DisplayPlatform::createSurface()
{
	VkSurfaceKHR surface;

	VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetPhysicalDeviceDisplayPropertiesKHR);
	VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
	VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetDisplayModePropertiesKHR);
	VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetDisplayPlaneSupportedDisplaysKHR);
	VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetDisplayPlaneCapabilitiesKHR);
	VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkCreateDisplayPlaneSurfaceKHR);

	// First, find all displays connected to this platform.
	uint32_t displayPropertyCount;
	VK_CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(gpu, &displayPropertyCount, nullptr));

	if (displayPropertyCount < 1)
	{
		LOGE("No displays available.\n");
		return VK_NULL_HANDLE;
	}

	vector<VkDisplayPropertiesKHR> displayProperties(displayPropertyCount);
	VK_CHECK(vkGetPhysicalDeviceDisplayPropertiesKHR(gpu, &displayPropertyCount, displayProperties.data()));

	// Find all supported planes.
	uint32_t planeCount;
	VK_CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(gpu, &planeCount, nullptr));

	if (planeCount < 1)
	{
		LOGE("No display planes available.\n");
		return VK_NULL_HANDLE;
	}

	vector<VkDisplayPlanePropertiesKHR> planeProperties(planeCount);
	VK_CHECK(vkGetPhysicalDeviceDisplayPlanePropertiesKHR(gpu, &planeCount, planeProperties.data()));

	struct Candidate
	{
		VkDisplayKHR display;
		VkDisplayModeKHR mode;
		uint32_t plane;
		uint32_t planeStack;
		uint32_t width;
		uint32_t height;
	};
	vector<Candidate> candidates;

	// Try to find a good combination of display mode, plane and display for use
	// with our application.
	for (uint32_t plane = 0; plane < planeCount; plane++)
	{
		uint32_t supportedCount;
		VK_CHECK(vkGetDisplayPlaneSupportedDisplaysKHR(gpu, plane, &supportedCount, nullptr));

		if (supportedCount < 1)
			continue;

		// For a given plane, find all displays which are supported.
		vector<VkDisplayKHR> supportedDisplays(supportedCount);
		VK_CHECK(vkGetDisplayPlaneSupportedDisplaysKHR(gpu, plane, &supportedCount, supportedDisplays.data()));

		for (auto display : supportedDisplays)
		{
			// Find the display properties belonging to this display.
			auto itr = find_if(begin(displayProperties), end(displayProperties),
			                   [display](const VkDisplayPropertiesKHR &props) { return props.display == display; });

			// This really shouldn't happen, since a plane cannot support a display
			// which doesn't exist.
			if (itr == end(displayProperties))
				continue;

			// Display should support identity transform.
			if ((itr->supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0)
				continue;

			// If the plane is associated with another display already, skip.
			if (planeProperties[plane].currentDisplay != display &&
			    planeProperties[plane].currentDisplay != VK_NULL_HANDLE)
				continue;

			// For the display, find all display modes.
			uint32_t modeCount;
			VK_CHECK(vkGetDisplayModePropertiesKHR(gpu, display, &modeCount, nullptr));

			if (modeCount < 1)
				continue;

			vector<VkDisplayModePropertiesKHR> modes(modeCount);
			VK_CHECK(vkGetDisplayModePropertiesKHR(gpu, display, &modeCount, modes.data()));

			for (auto mode : modes)
			{
				// Check that the mode we're trying to use supports what we need.
				VkDisplayPlaneCapabilitiesKHR caps;
				VK_CHECK(vkGetDisplayPlaneCapabilitiesKHR(gpu, mode.displayMode, plane, &caps));

				// We don't want alpha since we're not going to composite.
				if ((caps.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR) == 0)
					continue;

				// Check that our preferred swapchain fits into the plane.
				if (caps.minSrcExtent.width > preferredWidth)
					continue;
				if (caps.minSrcExtent.height > preferredHeight)
					continue;
				if (caps.maxSrcExtent.width < preferredWidth)
					continue;
				if (caps.maxSrcExtent.height < preferredHeight)
					continue;

				if (mode.parameters.visibleRegion.width >= preferredWidth &&
				    mode.parameters.visibleRegion.height >= preferredHeight)
				{
					// We found a candidate.
					candidates.push_back(
							{ display, mode.displayMode, plane, planeProperties[plane].currentStackIndex,
							  mode.parameters.visibleRegion.width, mode.parameters.visibleRegion.height });
				}
			}
		}
	}

	if (candidates.empty())
	{
		LOGE("Could not find a suitable display mode.\n");
		return VK_NULL_HANDLE;
	}

	// We could go though the list of candidates here to find the optimal match,
	// but we can pick the first one here.
	VkDisplaySurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR };
	info.displayMode = candidates.front().mode;
	info.planeIndex = candidates.front().plane;
	info.planeStackIndex = candidates.front().planeStack;
	info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	info.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
	info.imageExtent.width = candidates.front().width;
	info.imageExtent.height = candidates.front().height;

	LOGI("Using display mode: %u x %u.\n", info.imageExtent.width, info.imageExtent.height);

	VK_CHECK(vkCreateDisplayPlaneSurfaceKHR(instance, &info, nullptr, &surface));
	return surface;
}

Platform::Status DisplayPlatform::getWindowStatus()
{
	return status;
}

Platform::SwapchainDimensions DisplayPlatform::getPreferredSwapchain()
{
	SwapchainDimensions chain = { 1280, 720, VK_FORMAT_B8G8R8A8_UNORM };
	return chain;
}

Result DisplayPlatform::createWindow(const SwapchainDimensions &swapchain)
{
	preferredWidth = swapchain.width;
	preferredHeight = swapchain.height;
	return initVulkan(swapchain, { "VK_KHR_surface", "VK_KHR_display" }, { "VK_KHR_swapchain" });
}
}
