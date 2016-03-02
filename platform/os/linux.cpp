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

#include "framework/application.hpp"

#include "framework/common.hpp"
#include "platform/os.hpp"
#include "platform/platform.hpp"

#include "linux.hpp"
#include <limits.h>
#include <stdio.h>
#include <unistd.h>

using namespace std;

namespace MaliSDK
{
LinuxAssetManager::LinuxAssetManager()
{
	pid_t pid = getpid();
	char buf[PATH_MAX];

	static const char *exts[] = { "exe", "file", "path/a.out" };
	for (auto ext : exts)
	{
		string linkPath = "/proc/";
		linkPath += to_string(pid);
		linkPath += '/';
		linkPath += ext;

		ssize_t ret = readlink(linkPath.c_str(), buf, sizeof(buf) - 1);
		if (ret >= 0)
		{
			buf[ret] = '\0';
			basePath = buf;

			auto pos = basePath.find_last_of('/');
			if (pos == string::npos)
				basePath = ".";
			else
				basePath = basePath.substr(0, pos);

			LOGI("Found application base directory: \"%s\".\n", basePath.c_str());
			return;
		}
	}

	LOGE("Could not find application path based on /proc/$pid interface. Will "
	     "use working directory instead.\n");
	basePath = ".";
}

Result LinuxAssetManager::readBinaryFile(const char *pPath, void **ppData, size_t *pSize)
{
	auto fullpath = basePath + "/assets/" + pPath;
	return AssetManager::readBinaryFile(fullpath.c_str(), ppData, pSize);
}

AssetManager &OS::getAssetManager()
{
	static LinuxAssetManager manager;
	return manager;
}

double OS::getCurrentTime()
{
	timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
	{
		LOGE("clock_gettime() failed.\n");
		return 0.0;
	}

	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

unsigned OS::getNumberOfCpuThreads()
{
	long cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (cpus > 0)
	{
		LOGI("Detected %ld CPUs.\n", cpus);
		return unsigned(cpus);
	}
	else
	{
		LOGE("Failed to detect number of CPUs, assuming 1.\n");
		return 1;
	}
}
}

using namespace MaliSDK;

int main(int argc, char **argv)
{
	Platform &platform = Platform::get();
	if (FAILED(platform.initialize()))
	{
		LOGE("Failed to initialize platform.\n");
		return 1;
	}

	Platform::SwapchainDimensions dim = platform.getPreferredSwapchain();
	if (FAILED(platform.createWindow(dim)))
	{
		LOGE("Failed to create platform window.\n");
		return 1;
	}

	VulkanApplication *app = createApplication();
	if (!app)
	{
		LOGE("Failed to create application.\n");
		return 1;
	}

	if (!app->initialize(&platform.getContext()))
	{
		LOGE("Failed to initialize application.\n");
		return 1;
	}

	vector<VkImage> images;
	platform.getCurrentSwapchain(&images, &dim);
	app->updateSwapchain(images, dim);

	unsigned frameCount = 0;
	double startTime = OS::getCurrentTime();

	unsigned maxFrameCount = 0;
	bool useMaxFrameCount = false;
	if (argc == 2)
	{
		maxFrameCount = strtoul(argv[1], nullptr, 0);
		useMaxFrameCount = true;
	}

	while (platform.getWindowStatus() == Platform::STATUS_RUNNING)
	{
		unsigned index;
		Result res = platform.acquireNextImage(&index);
		while (res == RESULT_ERROR_OUTDATED_SWAPCHAIN)
		{
			res = platform.acquireNextImage(&index);
			platform.getCurrentSwapchain(&images, &dim);
			app->updateSwapchain(images, dim);
		}

		if (FAILED(res))
		{
			LOGE("Unrecoverable swapchain error.\n");
			break;
		}

		app->render(index, 0.0166f);
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

		if (useMaxFrameCount && (--maxFrameCount == 0))
			break;
	}

	app->terminate();
	delete app;
	platform.terminate();
}
