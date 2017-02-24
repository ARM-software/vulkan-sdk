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

#include "wsi.hpp"
#include <string.h>

using namespace std;

#ifdef FORCE_NO_VALIDATION
#define ENABLE_VALIDATION_LAYERS 0
#else
#define ENABLE_VALIDATION_LAYERS 1
#endif

namespace MaliSDK
{

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT type,
                                                    uint64_t object, size_t location, int32_t messageCode,
                                                    const char *pLayerPrefix, const char *pMessage, void *pUserData)
{
	auto *platform = static_cast<WSIPlatform *>(pUserData);
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

WSIPlatform::~WSIPlatform()
{
	terminate();
}

bool WSIPlatform::validateExtensions(const vector<const char *> &required,
                                     const std::vector<VkExtensionProperties> &available)
{
	for (auto extension : required)
	{
		bool found = false;
		for (auto &availableExtension : available)
		{
			if (strcmp(availableExtension.extensionName, extension) == 0)
			{
				found = true;
				break;
			}
		}

		if (!found)
			return false;
	}

	return true;
}

Result WSIPlatform::initialize()
{
	pContext = new Context();
	if (!pContext)
		return RESULT_ERROR_OUT_OF_MEMORY;

	return RESULT_SUCCESS;
}

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))
static const char *pValidationLayers[] = {
	"VK_LAYER_GOOGLE_threading",       "VK_LAYER_LUNARG_parameter_validation", "VK_LAYER_LUNARG_object_tracker",
	"VK_LAYER_LUNARG_core_validation", "VK_LAYER_LUNARG_device_limits",        "VK_LAYER_LUNARG_image",
	"VK_LAYER_LUNARG_swapchain",       "VK_LAYER_GOOGLE_unique_objects",
};

static const char *pMetaLayers[] = {
	"VK_LAYER_LUNARG_standard_validation",
};

static void addSupportedLayers(vector<const char *> &activeLayers, const vector<VkLayerProperties> &instanceLayers,
                               const char **ppRequested, unsigned numRequested)
{
	for (unsigned i = 0; i < numRequested; i++)
	{
		auto *layer = ppRequested[i];
		for (auto &ext : instanceLayers)
		{
			if (strcmp(ext.layerName, layer) == 0)
			{
				activeLayers.push_back(layer);
				break;
			}
		}
	}
}

Result WSIPlatform::initVulkan(const SwapchainDimensions &swapchain,
                               const vector<const char *> &requiredInstanceExtensions,
                               const vector<const char *> &requiredDeviceExtensions)
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
	vector<const char *> activeInstanceExtensions;

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

	// A layer could have VK_EXT_debug_report extension.
	for (auto &layer : instanceLayers)
	{
		uint32_t count;
		VK_CHECK(vkEnumerateInstanceExtensionProperties(layer.layerName, &count, nullptr));
		vector<VkExtensionProperties> extensions(count);
		VK_CHECK(vkEnumerateInstanceExtensionProperties(layer.layerName, &count, extensions.data()));
		for (auto &ext : extensions)
			instanceExtensions.push_back(ext);
	}

	// On desktop, the LunarG loader exposes a meta-layer that combines all
	// relevant validation layers.
	vector<const char *> activeLayers;
	addSupportedLayers(activeLayers, instanceLayers, pMetaLayers, NELEMS(pMetaLayers));

	// On Android, add all relevant layers one by one.
	if (activeLayers.empty())
	{
		addSupportedLayers(activeLayers, instanceLayers, pValidationLayers, NELEMS(pValidationLayers));
	}

	if (activeLayers.empty())
		LOGI("Did not find validation layers.\n");
	else
		LOGI("Found validation layers!\n");

	addExternalLayers(activeLayers, instanceLayers);
#endif

	bool useInstanceExtensions = true;
	if (!validateExtensions(requiredInstanceExtensions, instanceExtensions))
	{
		LOGI("Required instance extensions are missing, will try without.\n");
		useInstanceExtensions = false;
	}
	else
		activeInstanceExtensions = requiredInstanceExtensions;

	bool haveDebugReport = false;
	for (auto &ext : instanceExtensions)
	{
		if (strcmp(ext.extensionName, "VK_EXT_debug_report") == 0)
		{
			haveDebugReport = true;
			useInstanceExtensions = true;
			activeInstanceExtensions.push_back("VK_EXT_debug_report");
			break;
		}
	}

	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "Mali SDK";
	app.applicationVersion = 0;
	app.pEngineName = "Mali SDK";
	app.engineVersion = 0;
	app.apiVersion = VK_MAKE_VERSION(1, 0, 24);

	VkInstanceCreateInfo instanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	instanceInfo.pApplicationInfo = &app;
	if (useInstanceExtensions)
	{
		instanceInfo.enabledExtensionCount = activeInstanceExtensions.size();
		instanceInfo.ppEnabledExtensionNames = activeInstanceExtensions.data();
	}

#if ENABLE_VALIDATION_LAYERS
	if (!activeLayers.empty())
	{
		instanceInfo.enabledLayerCount = activeLayers.size();
		instanceInfo.ppEnabledLayerNames = activeLayers.data();
		LOGI("Using Vulkan instance validation layers.\n");
	}
#endif

	// Create the Vulkan instance
	{
		VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);

		// Try to fall back to compatible Vulkan versions if the driver is using
		// older, but compatible API versions.
		if (res == VK_ERROR_INCOMPATIBLE_DRIVER)
		{
			app.apiVersion = VK_MAKE_VERSION(1, 0, 1);
			res = vkCreateInstance(&instanceInfo, nullptr, &instance);
			if (res == VK_SUCCESS)
				LOGI("Created Vulkan instance with API version 1.0.1.\n");
		}

		if (res == VK_ERROR_INCOMPATIBLE_DRIVER)
		{
			app.apiVersion = VK_MAKE_VERSION(1, 0, 2);
			res = vkCreateInstance(&instanceInfo, nullptr, &instance);
			if (res == VK_SUCCESS)
				LOGI("Created Vulkan instance with API version 1.0.2.\n");
		}

		if (res != VK_SUCCESS)
		{
			LOGE("Failed to create Vulkan instance (error: %d).\n", int(res));
			return RESULT_ERROR_GENERIC;
		}
	}

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
	{
		LOGE("Failed to query number of queues.");
		return RESULT_ERROR_GENERIC;
	}

	uint32_t deviceExtensionCount;
	VK_CHECK(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deviceExtensionCount, nullptr));
	vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
	VK_CHECK(vkEnumerateDeviceExtensionProperties(gpu, nullptr, &deviceExtensionCount, deviceExtensions.data()));

#if ENABLE_VALIDATION_LAYERS
	uint32_t deviceLayerCount;
	VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, nullptr));
	vector<VkLayerProperties> deviceLayers(deviceLayerCount);
	VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, deviceLayers.data()));

	activeLayers.clear();
	// On desktop, the LunarG loader exposes a meta-layer that combines all
	// relevant validation layers.
	addSupportedLayers(activeLayers, deviceLayers, pMetaLayers, NELEMS(pMetaLayers));

	// On Android, add all relevant layers one by one.
	if (activeLayers.empty())
	{
		addSupportedLayers(activeLayers, deviceLayers, pValidationLayers, NELEMS(pValidationLayers));
	}
	addExternalLayers(activeLayers, deviceLayers);
#endif

	for (auto &deviceExt : deviceExtensions)
		LOGI("Device extension: %s\n", deviceExt.extensionName);

	bool useDeviceExtensions = true;
	if (!validateExtensions(requiredDeviceExtensions, deviceExtensions))
	{
		LOGI("Required device extensions are missing, will try without.\n");
		useDeviceExtensions = false;
	}

	if (FAILED(loadInstanceSymbols()))
	{
		LOGE("Failed to load instance symbols.");
		return RESULT_ERROR_GENERIC;
	}

	surface = createSurface();
	if (surface == VK_NULL_HANDLE)
	{
		LOGE("Failed to create surface.");
		return RESULT_ERROR_GENERIC;
	}

	bool foundQueue = false;
	for (unsigned i = 0; i < queueCount; i++)
	{
		VkBool32 supportsPresent;

		// There must exist at least one queue that has graphics and compute
		// support.
		const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

		vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supportsPresent);

		// We want a queue which supports all of graphics, compute and presentation.
		if (((queueProperties[i].queueFlags & required) == required) && supportsPresent)
		{
			graphicsQueueIndex = i;
			foundQueue = true;
			break;
		}
	}

	if (!foundQueue)
	{
		LOGE("Did not find suitable queue which supports graphics, compute and "
		     "presentation.\n");
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
	if (useDeviceExtensions)
	{
		deviceInfo.enabledExtensionCount = requiredDeviceExtensions.size();
		deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
	}

#if ENABLE_VALIDATION_LAYERS
	if (!activeLayers.empty())
	{
		deviceInfo.enabledLayerCount = activeLayers.size();
		deviceInfo.ppEnabledLayerNames = activeLayers.data();
		LOGI("Using Vulkan device validation layers.\n");
	}
#endif

	deviceInfo.pEnabledFeatures = &features;

	VK_CHECK(vkCreateDevice(gpu, &deviceInfo, nullptr, &device));

	if (!vulkanSymbolWrapperLoadCoreDeviceSymbols(device))
	{
		LOGE("Failed to load device symbols.");
		return RESULT_ERROR_GENERIC;
	}

	if (FAILED(loadDeviceSymbols()))
	{
		LOGE("Failed to load device symbols.");
		return RESULT_ERROR_GENERIC;
	}

	vkGetDeviceQueue(device, graphicsQueueIndex, 0, &queue);

	Result res = initSwapchain(swapchain);
	if (res != RESULT_SUCCESS)
	{
		LOGE("Failed to init swapchain.");
		return res;
	}

	res = pContext->onPlatformUpdate(this);
	if (FAILED(res))
		return res;

	semaphoreManager = new SemaphoreManager(device);
	return RESULT_SUCCESS;
}

void WSIPlatform::destroySwapchain()
{
	if (device)
		vkDeviceWaitIdle(device);

	if (swapchain)
	{
		vkDestroySwapchainKHR(device, swapchain, nullptr);
		swapchain = VK_NULL_HANDLE;
	}
}

void WSIPlatform::terminate()
{
	// Don't release anything until the GPU is completely idle.
	if (device)
		vkDeviceWaitIdle(device);

	delete semaphoreManager;
	semaphoreManager = nullptr;

	// Make sure we tear down the context before destroying the device since
	// context
	// also owns some Vulkan resources.
	delete pContext;
	pContext = nullptr;

	destroySwapchain();

	if (surface)
	{
		vkDestroySurfaceKHR(instance, surface, nullptr);
		surface = VK_NULL_HANDLE;
	}

	if (device)
	{
		vkDestroyDevice(device, nullptr);
		device = VK_NULL_HANDLE;
	}

	if (debug_callback)
	{
		VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkDestroyDebugReportCallbackEXT);
		vkDestroyDebugReportCallbackEXT(instance, debug_callback, nullptr);
		debug_callback = VK_NULL_HANDLE;
	}

	if (instance)
	{
		vkDestroyInstance(instance, nullptr);
		instance = VK_NULL_HANDLE;
	}
}

Result WSIPlatform::loadDeviceSymbols()
{
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(device, vkCreateSwapchainKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(device, vkDestroySwapchainKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(device, vkGetSwapchainImagesKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(device, vkAcquireNextImageKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(device, vkQueuePresentKHR))
		return RESULT_ERROR_GENERIC;
	return RESULT_SUCCESS;
}

Result WSIPlatform::loadInstanceSymbols()
{
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetPhysicalDeviceSurfaceSupportKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetPhysicalDeviceSurfaceFormatsKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkGetPhysicalDeviceSurfacePresentModesKHR))
		return RESULT_ERROR_GENERIC;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance, vkDestroySurfaceKHR))
		return RESULT_ERROR_GENERIC;
	return RESULT_SUCCESS;
}

Result WSIPlatform::initSwapchain(const SwapchainDimensions &dim)
{
	VkSurfaceCapabilitiesKHR surfaceProperties;
	VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surfaceProperties));

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
	vector<VkSurfaceFormatKHR> formats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, formats.data());

	VkSurfaceFormatKHR format;
	if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
	{
		format = formats[0];
		format.format = dim.format;
	}
	else
	{
		if (formatCount == 0)
		{
			LOGE("Surface has no formats.\n");
			return RESULT_ERROR_GENERIC;
		}

		format.format = VK_FORMAT_UNDEFINED;
		for (auto &candidate : formats)
		{
			switch (candidate.format)
			{
				// Favor UNORM formats as the samples are not written for sRGB currently.
				case VK_FORMAT_R8G8B8A8_UNORM:
				case VK_FORMAT_B8G8R8A8_UNORM:
				case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
					format = candidate;
					break;

				default:
					break;
			}

			if (format.format != VK_FORMAT_UNDEFINED)
				break;
		}

		if (format.format == VK_FORMAT_UNDEFINED)
			format = formats[0];
	}

	VkExtent2D swapchainSize;
	// -1u is a magic value (in Vulkan specification) which means there's no fixed
	// size.
	if (surfaceProperties.currentExtent.width == -1u)
	{
		swapchainSize.width = dim.width;
		swapchainSize.height = dim.height;
	}
	else
		swapchainSize = surfaceProperties.currentExtent;

	// FIFO must be supported by all implementations.
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	// Determine the number of VkImage's to use in the swapchain.
	// Ideally, we desire to own 1 image at a time, the rest of the images can
	// either be rendered to and/or
	// being queued up for display.
	uint32_t desiredSwapchainImages = surfaceProperties.minImageCount + 1;
	if ((surfaceProperties.maxImageCount > 0) && (desiredSwapchainImages > surfaceProperties.maxImageCount))
	{
		// Application must settle for fewer images than desired.
		desiredSwapchainImages = surfaceProperties.maxImageCount;
	}

	// Figure out a suitable surface transform.
	VkSurfaceTransformFlagBitsKHR preTransform;
	if (surfaceProperties.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	else
		preTransform = surfaceProperties.currentTransform;

	VkSwapchainKHR oldSwapchain = swapchain;

	// Find a supported composite type.
	VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
		composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	else if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
		composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	else if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
		composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
	else if (surfaceProperties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
		composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

	VkSwapchainCreateInfoKHR info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	info.surface = surface;
	info.minImageCount = desiredSwapchainImages;
	info.imageFormat = format.format;
	info.imageColorSpace = format.colorSpace;
	info.imageExtent.width = swapchainSize.width;
	info.imageExtent.height = swapchainSize.height;
	info.imageArrayLayers = 1;
	info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.preTransform = preTransform;
	info.compositeAlpha = composite;
	info.presentMode = swapchainPresentMode;
	info.clipped = true;
	info.oldSwapchain = oldSwapchain;

	VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain));

	if (oldSwapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

	swapchainDimensions.width = swapchainSize.width;
	swapchainDimensions.height = swapchainSize.height;
	swapchainDimensions.format = format.format;

	uint32_t imageCount;
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
	swapchainImages.resize(imageCount);
	VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()));

	return RESULT_SUCCESS;
}

void WSIPlatform::getCurrentSwapchain(vector<VkImage> *images, SwapchainDimensions *swapchain)
{
	*images = swapchainImages;
	*swapchain = swapchainDimensions;
}

unsigned WSIPlatform::getNumSwapchainImages() const
{
	return swapchainImages.size();
}

Result WSIPlatform::acquireNextImage(unsigned *image)
{
	auto acquireSemaphore = semaphoreManager->getClearedSemaphore();
	VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquireSemaphore, VK_NULL_HANDLE, image);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
	{
		vkQueueWaitIdle(queue);
		semaphoreManager->addClearedSemaphore(acquireSemaphore);

		// Recreate swapchain.
		if (SUCCEEDED(initSwapchain(swapchainDimensions)))
			return RESULT_ERROR_OUTDATED_SWAPCHAIN;
		else
			return RESULT_ERROR_GENERIC;
	}
	else if (res != VK_SUCCESS)
	{
		vkQueueWaitIdle(queue);
		semaphoreManager->addClearedSemaphore(acquireSemaphore);
		return RESULT_ERROR_GENERIC;
	}
	else
	{
		// Signal the underlying context that we're using this backbuffer now.
		// This will also wait for all fences associated with this swapchain image
		// to complete first.
		// When submitting command buffer that writes to swapchain, we need to wait
		// for this semaphore first.
		// Also, delete the older semaphore.
		auto oldSemaphore = pContext->beginFrame(*image, acquireSemaphore);

		// Recycle the old semaphore back into the semaphore manager.
		if (oldSemaphore != VK_NULL_HANDLE)
			semaphoreManager->addClearedSemaphore(oldSemaphore);

		return RESULT_SUCCESS;
	}
}

Result WSIPlatform::presentImage(unsigned index)
{
	VkResult result;
	VkPresentInfoKHR present = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present.swapchainCount = 1;
	present.pSwapchains = &swapchain;
	present.pImageIndices = &index;
	present.pResults = &result;
	present.waitSemaphoreCount = 1;
	present.pWaitSemaphores = &pContext->getSwapchainReleaseSemaphore();

	VkResult res = vkQueuePresentKHR(queue, &present);

	if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
		return RESULT_ERROR_OUTDATED_SWAPCHAIN;
	else if (res != VK_SUCCESS)
		return RESULT_ERROR_GENERIC;
	else
		return RESULT_SUCCESS;
}
}
