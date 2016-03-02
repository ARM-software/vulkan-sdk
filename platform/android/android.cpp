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

#include "android.hpp"
using namespace std;

namespace MaliSDK
{
Platform &Platform::get()
{
	// Not initialized until first call to Platform::get().
	// Initialization is thread-safe.
	static AndroidPlatform singleton;
	return singleton;
}

Platform::Status AndroidPlatform::getWindowStatus()
{
	return STATUS_RUNNING;
}

VkSurfaceKHR AndroidPlatform::createSurface()
{
	VkSurfaceKHR surface;
	PFN_vkCreateAndroidSurfaceKHR fpCreateAndroidSurfaceKHR;
	if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(instance, "vkCreateAndroidSurfaceKHR", fpCreateAndroidSurfaceKHR))
	{
		LOGE("Couldn't find android surface creation symbol.");
		return VK_NULL_HANDLE;
	}

	VkAndroidSurfaceCreateInfoKHR info = { VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR };
	info.window = pNativeWindow;

	VK_CHECK(fpCreateAndroidSurfaceKHR(instance, &info, nullptr, &surface));
	return surface;
}

AssetManager &MaliSDK::OS::getAssetManager()
{
	static AndroidAssetManager manager;
	return manager;
}

double MaliSDK::OS::getCurrentTime()
{
	timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
	{
		LOGE("clock_gettime() failed.\n");
		return 0.0;
	}

	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

unsigned MaliSDK::OS::getNumberOfCpuThreads()
{
	unsigned count = android_getCpuCount();
	LOGI("Detected %u CPUs.\n", count);
	return count;
}

Platform::SwapchainDimensions AndroidPlatform::getPreferredSwapchain()
{
	SwapchainDimensions chain = { 1280, 720, VK_FORMAT_B8G8R8A8_UNORM };
	return chain;
}

Result AndroidPlatform::createWindow(const SwapchainDimensions &swapchain)
{
	return initVulkan(swapchain, { "VK_KHR_surface", "VK_KHR_android_surface" }, { "VK_KHR_swapchain" });
}

void AndroidPlatform::onResume(const SwapchainDimensions &swapchain)
{
	vkDeviceWaitIdle(device);
	initSwapchain(swapchain);
}

void AndroidPlatform::onPause()
{
	destroySwapchain();
}

void AndroidPlatform::terminate()
{
	WSIPlatform::terminate();
}
}

using namespace MaliSDK;

static int32_t engineHandleInput(android_app *, AInputEvent *)
{
	return 0;
}

static void engineHandleCmd(android_app *pApp, int32_t cmd)
{
	auto *state = static_cast<AndroidState *>(pApp->userData);
	auto &platform = static_cast<AndroidPlatform &>(Platform::get());

	switch (cmd)
	{
	case APP_CMD_RESUME:
	{
		state->active = true;
		Platform::SwapchainDimensions dim = platform.getPreferredSwapchain();

		if (state->pVulkanApp)
		{
			LOGI("Resuming swapchain!\n");
			platform.onResume(dim);

			vector<VkImage> images;
			platform.getCurrentSwapchain(&images, &dim);
			state->pVulkanApp->updateSwapchain(images, dim);
		}
		break;
	}

	case APP_CMD_PAUSE:
	{
		LOGI("Pausing swapchain!\n");
		state->active = false;
		platform.onPause();
		break;
	}

	case APP_CMD_INIT_WINDOW:
		if (pApp->window != nullptr)
		{
			LOGI("Initializing platform!\n");
			if (FAILED(platform.initialize()))
			{
				LOGE("Failed to initialize platform.\n");
				abort();
			}

			auto &platform = static_cast<AndroidPlatform &>(Platform::get());
			platform.setNativeWindow(state->pApp->window);

			Platform::SwapchainDimensions dim = platform.getPreferredSwapchain();

			LOGI("Creating window!\n");
			if (FAILED(platform.createWindow(dim)))
			{
				LOGE("Failed to create Vulkan window.\n");
				abort();
			}

			LOGI("Creating application!\n");
			state->pVulkanApp = createApplication();

			LOGI("Initializing application!\n");
			if (!state->pVulkanApp->initialize(&platform.getContext()))
			{
				LOGE("Failed to initialize Vulkan application.\n");
				abort();
			}

			LOGI("Updating swapchain!\n");
			vector<VkImage> images;
			platform.getCurrentSwapchain(&images, &dim);
			state->pVulkanApp->updateSwapchain(images, dim);
		}
		break;

	case APP_CMD_TERM_WINDOW:
		if (state->pVulkanApp)
		{
			LOGI("Terminating application!\n");
			state->pVulkanApp->terminate();
			delete state->pVulkanApp;
			state->pVulkanApp = nullptr;
			platform.terminate();
		}
		break;
	}
}

void android_main(android_app *state)
{
	LOGI("Entering android_main()!\n");

	// Make sure glue is not stripped.
	app_dummy();

	AndroidState engine;
	memset(&engine, 0, sizeof(engine));
	engine.pApp = state;

	state->userData = &engine;
	state->onAppCmd = engineHandleCmd;
	state->onInputEvent = engineHandleInput;
	engine.pApp = state;

	auto &platform = static_cast<AndroidPlatform &>(Platform::get());
	static_cast<AndroidAssetManager &>(OS::getAssetManager()).setAssetManager(state->activity->assetManager);

	unsigned frameCount = 0;
	double startTime = OS::getCurrentTime();

	for (;;)
	{
		struct android_poll_source *source;
		int ident;
		int events;

		while ((ident = ALooper_pollAll(engine.pVulkanApp && engine.active ? 0 : -1, nullptr, &events,
		                                (void **)&source)) >= 0)
		{
			if (source)
				source->process(state, source);

			if (state->destroyRequested)
				return;
		}

		if (engine.pVulkanApp && engine.active)
		{
			unsigned index;
			vector<VkImage> images;
			Platform::SwapchainDimensions dim;

			Result res = platform.acquireNextImage(&index);
			while (res == RESULT_ERROR_OUTDATED_SWAPCHAIN)
			{
				platform.acquireNextImage(&index);
				platform.getCurrentSwapchain(&images, &dim);
				engine.pVulkanApp->updateSwapchain(images, dim);
			}

			if (FAILED(res))
			{
				LOGE("Unrecoverable swapchain error.\n");
				break;
			}

			engine.pVulkanApp->render(index, 0.0166f);
			res = platform.presentImage(index);

			// Handle Outdated error in acquire.
			if (FAILED(res) && res != RESULT_ERROR_OUTDATED_SWAPCHAIN)
				break;

			frameCount++;
			if (frameCount == 100)
			{
				double endTime = OS::getCurrentTime();
				LOGI("FPS: %.3f\n", frameCount / (endTime - startTime));
				frameCount = 0;
				startTime = endTime;
			}
		}
	}
}
