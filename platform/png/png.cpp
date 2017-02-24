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

#include "png.hpp"
#include <stdlib.h>
#include <string.h>

#define PNG_SWAPCHAIN_IMAGES 3

#ifdef FORCE_NO_VALIDATION
#define ENABLE_VALIDATION_LAYERS 0
#else
#define ENABLE_VALIDATION_LAYERS 1
#endif

using namespace std;

namespace MaliSDK
{

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT type,
                                                    uint64_t object, size_t location, int32_t messageCode,
                                                    const char *pLayerPrefix, const char *pMessage, void *pUserData)
{
	auto *platform = static_cast<PNGPlatform *>(pUserData);
	auto callback = platform->getExternalDebugCallback();

	if (callback)
	{
		return callback(flags, type, object, location, messageCode, pLayerPrefix, pMessage,
		                platform->getExternalDebugCallbackUserData());
	}
	else
	{
		if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		{
			LOGE("Validation Layer: Error: %s: %s\n", pLayerPrefix, pMessage);
		}
		else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		{
			LOGE("Validation Layer: Warning: %s: %s\n", pLayerPrefix, pMessage);
		}
		else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		{
			LOGI("Validation Layer: Performance warning: %s: %s\n", pLayerPrefix, pMessage);
		}
		else
		{
			LOGI("Validation Layer: Information: %s: %s\n", pLayerPrefix, pMessage);
		}
		return VK_FALSE;
	}
}

uint32_t PNGPlatform::findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements)
{
	const VkPhysicalDeviceMemoryProperties &props = memoryProperties;
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if (deviceRequirements & (1u << i))
		{
			if ((props.memoryTypes[i].propertyFlags & hostRequirements) == hostRequirements)
			{
				return i;
			}
		}
	}

	LOGE("Failed to obtain suitable memory type.\n");
	abort();
}

uint32_t PNGPlatform::findMemoryTypeFromRequirementsFallback(uint32_t deviceRequirements, uint32_t hostRequirements,
                                                             uint32_t hostRequirementsFallback)
{
	const VkPhysicalDeviceMemoryProperties &props = memoryProperties;
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if (deviceRequirements & (1u << i))
		{
			if ((props.memoryTypes[i].propertyFlags & hostRequirements) == hostRequirements)
			{
				return i;
			}
		}
	}

	return findMemoryTypeFromRequirements(deviceRequirements, hostRequirementsFallback);
}

Platform &Platform::get()
{
	// Not initialized until first call to Platform::get().
	// Initialization is thread-safe.
	static PNGPlatform singleton;
	return singleton;
}

Platform::Status PNGPlatform::getWindowStatus()
{
	return STATUS_RUNNING;
}

Result PNGPlatform::initialize()
{
	const char *path = getenv("MALI_PNG_PATH");
	if (!path)
	{
		LOGI("MALI_PNG_PATH environment variable not defined, falling back to "
		     "default.\n");
		path = "Mali-SDK-Frames";
	}
	LOGI("Dumping PNG files to: %s.xxxxxxxx.png.\n", path);

	pngSwapchain = new PNGSwapchain;
	if (!pngSwapchain)
		return RESULT_ERROR_OUT_OF_MEMORY;

	// Create a custom swapchain.
	if (FAILED(pngSwapchain->init(path, PNG_SWAPCHAIN_IMAGES)))
		return RESULT_ERROR_GENERIC;

	pContext = new Context();
	if (!pContext)
		return RESULT_ERROR_OUT_OF_MEMORY;

	return RESULT_SUCCESS;
}

void PNGPlatform::terminate()
{
	// Don't release anything until the GPU is completely idle.
	if (device)
		vkDeviceWaitIdle(device);

	// Make sure we delete the PNG swapchain before tearing down the buffers.
	delete pngSwapchain;
	pngSwapchain = nullptr;

	for (auto &image : swapchainImages)
		if (image != VK_NULL_HANDLE)
			vkDestroyImage(device, image, nullptr);

	for (auto &memory : swapchainMemory)
		if (memory != VK_NULL_HANDLE)
			vkFreeMemory(device, memory, nullptr);

	for (auto &buffer : swapchainReadback)
		if (buffer != VK_NULL_HANDLE)
			vkDestroyBuffer(device, buffer, nullptr);

	for (auto &memory : swapchainReadbackMemory)
		if (memory != VK_NULL_HANDLE)
			vkFreeMemory(device, memory, nullptr);

	// Make sure we tear down the context before destroying the device since
	// context
	// also owns some Vulkan resources.
	delete pContext;
	pContext = nullptr;

	if (device)
		vkDestroyDevice(device, nullptr);

	if (debug_callback)
	{
		VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkDestroyDebugReportCallbackEXT);
		vkDestroyDebugReportCallbackEXT(instance, debug_callback, nullptr);
	}

	if (instance)
		vkDestroyInstance(instance, nullptr);

	swapchainImages.clear();
	swapchainMemory.clear();
	swapchainReadback.clear();
	swapchainReadbackMemory.clear();
	device = VK_NULL_HANDLE;
	debug_callback = VK_NULL_HANDLE;
	instance = VK_NULL_HANDLE;
}

PNGPlatform::~PNGPlatform()
{
	terminate();
}

Platform::SwapchainDimensions PNGPlatform::getPreferredSwapchain()
{
	SwapchainDimensions chain = {
		1280, 720, VK_FORMAT_R8G8B8A8_UNORM,
	};

	return chain;
}

Result PNGPlatform::createWindow(const SwapchainDimensions &swapchain)
{
	return initVulkan(swapchain);
}

void PNGPlatform::getCurrentSwapchain(vector<VkImage> *images, SwapchainDimensions *swapchain)
{
	*images = swapchainImages;
	*swapchain = swapchainDimensions;
}

unsigned PNGPlatform::getNumSwapchainImages() const
{
	return swapchainImages.size();
}

Result PNGPlatform::acquireNextImage(unsigned *image)
{
	// This function will return when scanout is complete.
	// This is similar to vkAcquireNextImageKHR and we wait for the fence it
	// provides us.
	*image = pngSwapchain->acquire();
	// Signal the underlying context that we're using this backbuffer now.
	// This will also wait for all fences associated with this swapchain image to
	// complete first.
	pContext->beginFrame(*image, VK_NULL_HANDLE);
	return RESULT_SUCCESS;
}

void PNGPlatform::imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
                                     VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
                                     VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout,
                                     VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, false, 0, nullptr, 0, nullptr, 1, &barrier);
}

Result PNGPlatform::presentImage(unsigned index)
{
	auto cmd = pContext->requestPrimaryCommandBuffer();

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Transition image back from PRESENT_SRC_KHR to TRANSFER_SRC_OPTIMAL.
	imageMemoryBarrier(cmd, swapchainImages[index], VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT,
	                   VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                   VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	// Copy from the image to a host-visible buffer.
	VkBufferImageCopy region = { 0 };
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = swapchainDimensions.width;
	region.imageExtent.height = swapchainDimensions.height;
	region.imageExtent.depth = 1;
	vkCmdCopyImageToBuffer(cmd, swapchainImages[index], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchainReadback[index],
	                       1, &region);

	VK_CHECK(vkEndCommandBuffer(cmd));
	pContext->submit(cmd);

	// For the PNG swapchain, we rely on keeping track of all the queue
	// submissions we made,
	// wait for their fences to hit in a separate thread, then we can do
	// multithreaded PNG encoding.
	unsigned numFences = pContext->getFenceManager().getActiveFenceCount();
	VkFence *fences = pContext->getFenceManager().getActiveFences();
	pngSwapchain->present(index, device, swapchainReadbackMemory[index], swapchainDimensions.width,
	                      swapchainDimensions.height, numFences, fences, swapchainCoherent);
	return RESULT_SUCCESS;
}

Result PNGPlatform::initVulkan(const SwapchainDimensions &swapchain)
{
	if (!vulkanSymbolWrapperInitLoader())
	{
		LOGE("Cannot find Vulkan loader.\n");
		return RESULT_ERROR_GENERIC;
	}

	if (!vulkanSymbolWrapperLoadGlobalSymbols())
	{
		LOGE("Failed to load global Vulkan symbols.\n");
		return RESULT_ERROR_GENERIC;
	}

	uint32_t instanceExtensionCount;
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));
	vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
	VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data()));

	for (auto &instanceExt : instanceExtensions)
		LOGI("Instance extension: %s\n", instanceExt.extensionName);

#if ENABLE_VALIDATION_LAYERS
	uint32_t instanceLayerCount;
	VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
	vector<VkLayerProperties> instanceLayers(instanceLayerCount);
	VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayers.data()));

	vector<const char *> activeLayers;
	for (auto &ext : instanceLayers)
		if (strcmp(ext.layerName, "VK_LAYER_LUNARG_standard_validation") == 0)
			activeLayers.push_back("VK_LAYER_LUNARG_standard_validation");

	if (activeLayers.empty())
		LOGI("Did not find validation layers.\n");
	else
		LOGI("Found validation layers!\n");

	addExternalLayers(activeLayers, instanceLayers);
#endif

	bool haveDebugReport = false;
	vector<const char *> activeInstanceExtensions;
	for (auto &ext : instanceExtensions)
	{
		if (strcmp(ext.extensionName, "VK_EXT_debug_report") == 0)
		{
			haveDebugReport = true;
			activeInstanceExtensions.push_back("VK_EXT_debug_report");
			break;
		}
	}

	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "Mali SDK";
	app.applicationVersion = 0;
	app.pEngineName = "Mali SDK";
	app.engineVersion = 0;
	app.apiVersion = VK_MAKE_VERSION(1, 0, 13);

	VkInstanceCreateInfo info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	info.pApplicationInfo = &app;

#if ENABLE_VALIDATION_LAYERS
	if (!activeLayers.empty())
	{
		info.enabledLayerCount = activeLayers.size();
		info.ppEnabledLayerNames = activeLayers.data();
		LOGI("Using Vulkan instance validation layers.\n");
	}

	if (!activeInstanceExtensions.empty())
	{
		info.enabledExtensionCount = activeInstanceExtensions.size();
		info.ppEnabledExtensionNames = activeInstanceExtensions.data();
	}
#endif

	VK_CHECK(vkCreateInstance(&info, nullptr, &instance));

	if (!vulkanSymbolWrapperLoadCoreInstanceSymbols(instance))
	{
		LOGE("Failed to load instance symbols.");
		return RESULT_ERROR_GENERIC;
	}

	if (haveDebugReport)
	{
		VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkCreateDebugReportCallbackEXT);
		VkDebugReportCallbackCreateInfoEXT info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
		info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
		             VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;

		info.pfnCallback = debugCallback;
		info.pUserData = this;
		if (vkCreateDebugReportCallbackEXT)
			vkCreateDebugReportCallbackEXT(instance, &info, nullptr, &debug_callback);
		LOGI("Enabling Vulkan debug reporting.\n");
	}

	uint32_t gpuCount = 0;
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
	if (gpuCount < 1)
	{
		LOGE("Failed to enumerate Vulkan physical device.\n");
		return RESULT_ERROR_GENERIC;
	}

	vector<VkPhysicalDevice> gpus(gpuCount);
	VK_CHECK(vkEnumeratePhysicalDevices(instance, &gpuCount, gpus.data()));

	gpu = VK_NULL_HANDLE;

	for (auto device : gpus)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(device, &properties);

		// If we have multiple GPUs in our system, try to find a Mali device.
		if (strstr(properties.deviceName, "Mali"))
		{
			gpu = device;
			LOGI("Found ARM Mali physical device: %s.\n", properties.deviceName);
			break;
		}
	}

	// Fallback to the first GPU we find in the system.
	if (gpu == VK_NULL_HANDLE)
		gpu = gpus.front();

	vkGetPhysicalDeviceProperties(gpu, &gpuProperties);
	vkGetPhysicalDeviceMemoryProperties(gpu, &memoryProperties);

	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, nullptr);
	queueProperties.resize(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueCount, queueProperties.data());
	if (queueCount < 1)
		return RESULT_ERROR_GENERIC;

#if ENABLE_VALIDATION_LAYERS
	uint32_t deviceLayerCount;
	VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, nullptr));
	vector<VkLayerProperties> deviceLayers(deviceLayerCount);
	VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, deviceLayers.data()));

	activeLayers.clear();
	for (auto &ext : deviceLayers)
	{
		if (strcmp(ext.layerName, "VK_LAYER_LUNARG_standard_validation") == 0)
			activeLayers.push_back("VK_LAYER_LUNARG_standard_validation");
	}

	addExternalLayers(activeLayers, instanceLayers);
#endif

	bool foundQueue = false;
	for (unsigned i = 0; i < queueCount; i++)
	{
		if (queueProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			graphicsQueueIndex = i;
			foundQueue = true;
			break;
		}
	}

	if (!foundQueue)
	{
		LOGE("Did not find suitable graphics queue.\n");
		return RESULT_ERROR_GENERIC;
	}

	static const float one = 1.0f;
	VkDeviceQueueCreateInfo queueInfo = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	queueInfo.queueFamilyIndex = graphicsQueueIndex;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &one;

	VkPhysicalDeviceFeatures features = { false };
	VkDeviceCreateInfo deviceInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;
	deviceInfo.pEnabledFeatures = &features;

#if ENABLE_VALIDATION_LAYERS
	if (!activeLayers.empty())
	{
		deviceInfo.enabledLayerCount = activeLayers.size();
		deviceInfo.ppEnabledLayerNames = activeLayers.data();
		LOGI("Using Vulkan device validation layers.\n");
	}
#endif

	VK_CHECK(vkCreateDevice(gpu, &deviceInfo, nullptr, &device));
	if (!vulkanSymbolWrapperLoadCoreDeviceSymbols(device))
	{
		LOGE("Failed to load device symbols.");
		return RESULT_ERROR_GENERIC;
	}

	vkGetDeviceQueue(device, graphicsQueueIndex, 0, &queue);

	swapchainDimensions = swapchain;
	swapchainDimensions.format = VK_FORMAT_R8G8B8A8_UNORM;
	swapchainImages.resize(pngSwapchain->getNumImages());
	swapchainMemory.resize(pngSwapchain->getNumImages());
	swapchainReadback.resize(pngSwapchain->getNumImages());
	swapchainReadbackMemory.resize(pngSwapchain->getNumImages());

	for (unsigned i = 0; i < pngSwapchain->getNumImages(); i++)
	{
		VkImageCreateInfo image = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = VK_FORMAT_R8G8B8A8_UNORM;
		image.extent.width = swapchainDimensions.width;
		image.extent.height = swapchainDimensions.height;
		image.extent.depth = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		image.mipLevels = 1;
		image.arrayLayers = 1;

		VK_CHECK(vkCreateImage(device, &image, nullptr, &swapchainImages[i]));

		// Allocate memory for the texture.
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(device, swapchainImages[i], &memReqs);

		VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
		alloc.allocationSize = memReqs.size;
		alloc.memoryTypeIndex =
		    findMemoryTypeFromRequirements(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &swapchainMemory[i]));
		vkBindImageMemory(device, swapchainImages[i], swapchainMemory[i], 0);

		// Create a buffer which we will read back from.
		VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		bufferInfo.size = swapchainDimensions.width * swapchainDimensions.height * 4;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &swapchainReadback[i]));
		vkGetBufferMemoryRequirements(device, swapchainReadback[i], &memReqs);

		alloc.allocationSize = memReqs.size;
		// Try to use CACHED_BIT if available, since this will greatly accelerate
		// readbacks.
		alloc.memoryTypeIndex = findMemoryTypeFromRequirementsFallback(
		    memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
		    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &swapchainReadbackMemory[i]));

		// Figure out if we have incoherent memory. If so, we need to do cache
		// control manually.
		// This will be the same for every iteration.
		swapchainCoherent = (memoryProperties.memoryTypes[alloc.memoryTypeIndex].propertyFlags &
		                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;

		vkBindBufferMemory(device, swapchainReadback[i], swapchainReadbackMemory[i], 0);
	}

	Result res = pContext->onPlatformUpdate(this);
	if (FAILED(res))
		return RESULT_ERROR_GENERIC;

	return RESULT_SUCCESS;
}
}
