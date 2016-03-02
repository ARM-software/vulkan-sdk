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

#include "png_swapchain.hpp"
#include <assert.h>
#include <string.h>

#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "framework/stb/stb_image_write.h"

using namespace MaliSDK;
using namespace std;

Result PNGSwapchain::init(const char *pBasePath, unsigned swapchainImagesCount)
{
	basePath = pBasePath;
	this->swapchainImagesCount = swapchainImagesCount;

	for (unsigned i = 0; i < swapchainImagesCount; i++)
		vacant.push(i);

	worker = thread(&PNGSwapchain::threadEntry, this);
	return RESULT_SUCCESS;
}

void PNGSwapchain::join()
{
	if (worker.joinable())
	{
		lock.lock();
		dead = true;
		cond.notify_all();
		lock.unlock();
		worker.join();
	}
}

PNGSwapchain::~PNGSwapchain()
{
	join();
}

void PNGSwapchain::present(unsigned index, VkDevice device, VkDeviceMemory memory, unsigned width, unsigned height,
                           unsigned numFences, VkFence *fences, bool coherent)
{
	lock_guard<mutex> l{ lock };
	ready.push({ device, memory, fences, numFences, index, width, height, coherent });
	cond.notify_all();
}

unsigned PNGSwapchain::acquire()
{
	unique_lock<mutex> l{ lock };
	cond.wait(l, [this] { return !vacant.empty(); });
	unsigned index = vacant.front();
	vacant.pop();
	return index;
}

void PNGSwapchain::dump(const Command &cmd, unsigned sequenceCount)
{
	char formatted[64];
	sprintf(formatted, ".%08u.png", sequenceCount);
	string path = basePath + formatted;

	LOGI("Writing PNG file to: \"%s\".\n", path.c_str());

	void *pSrc = nullptr;
	VK_CHECK(vkMapMemory(cmd.device, cmd.memory, 0, cmd.width * cmd.height * 4, 0, &pSrc));

	// If our memory is incoherent, make sure that we invalidate the CPU caches
	// before copying.
	if (!cmd.coherent)
	{
		VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
		range.memory = cmd.memory;
		range.size = VK_WHOLE_SIZE;
		VK_CHECK(vkInvalidateMappedMemoryRanges(cmd.device, 1, &range));
	}

	int ret = stbi_write_png(path.c_str(), cmd.width, cmd.height, 4, pSrc, cmd.width * 4);
	vkUnmapMemory(cmd.device, cmd.memory);

	if (ret != 0)
		LOGI("Wrote PNG file: \"%s\".\n", path.c_str());
	else
		LOGE("Failed to write PNG file: \"%s\".\n", path.c_str());
}

void PNGSwapchain::threadEntry()
{
	// Very basic approach. Application will push render requests into a
	// thread-safe queue.
	// This thread will wait for the relevant fences to complete, then dump the
	// PNG to disk.
	// We then make the buffer that was being scanned out available to the
	// application.
	//
	// We could make the buffer available before we wait for the fences, but we
	// would then have to provide
	// a semaphore + fence just like Vulkan WSI, that would be quite complicated
	// since that generally
	// would require driver support.

	unsigned sequenceCount = 0;

	for (;;)
	{
		Command command;
		{
			unique_lock<mutex> l{ lock };
			cond.wait(l, [this] { return !ready.empty() || dead; });

			if (dead)
				break;

			command = ready.front();
			ready.pop();
		}

		vkWaitForFences(command.device, command.numFences, command.fences, true, UINT64_MAX);
		unsigned nextVacant = displayed;

		dump(command, sequenceCount++);

		// Previous buffer is now ready to be rendered into.
		if (nextVacant != command.index)
		{
			lock_guard<mutex> l{ lock };
			vacant.push(nextVacant);
			cond.notify_all();
		}

		displayed = command.index;
	}
}
