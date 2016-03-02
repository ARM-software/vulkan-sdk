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

#ifndef DMABUF_SWAPCHAIN_HPP
#define DMABUF_SWAPCHAIN_HPP

#include "libvulkan-stub.h"
#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

#include "framework/common.hpp"

namespace MaliSDK
{

/// @brief This class implements a swapchain outside the Vulkan API.
/// Its main purpose is debugging without a screen since the swapchain will dump
/// output
/// directly to PNG files instead of displaying on-screen.
class PNGSwapchain
{
public:
	/// @brief Initialize the swapchain.
	/// @param pBasePath The base path that will be used for all PNG images.
	/// @param swapchainImagesCount The number of swapchain images to create in
	/// the internal queue.
	/// @returns Error code.
	Result init(const char *pBasePath, unsigned swapchainImagesCount);

	/// @brief Destructor
	~PNGSwapchain();

	/// @brief Gets number of images in the swapchain.
	/// @returns Number of swapchain images.
	inline unsigned getNumImages() const
	{
		return swapchainImagesCount;
	}

	/// @brief Dump image for a swapchain index to disk.
	/// @param index Index to present.
	/// @param device Vulkan device.
	/// @param memory The VkDeviceMemory associated with the swapchain image. The
	/// memory must be tightly packed in VK_FORMAT_R8G8B8A8_UNORM format.
	/// @param width The width of the swapchain image.
	/// @param height The height of the swapchain image.
	/// @param numFences The number of VkFences to wait on before dumping the
	/// texture.
	/// @param[in] fences Fences to wait for.
	/// @param coherent If the swapchain memory is coherent, i.e. does not need to
	/// invalidate caches.
	void present(unsigned index, VkDevice device, VkDeviceMemory memory, unsigned width, unsigned height,
	             unsigned numFences, VkFence *fences, bool coherent);

	/// @brief Acquire a new swapchain index.
	/// When acquire returns the image is ready to be presented into, so no
	/// semaphores
	/// are required.
	unsigned acquire();

private:
	std::thread worker;
	unsigned swapchainImagesCount;
	std::string basePath;

	struct Command
	{
		VkDevice device;
		VkDeviceMemory memory;
		VkFence *fences;
		unsigned numFences;
		unsigned index;
		unsigned width;
		unsigned height;
		bool coherent;
	};

	std::queue<unsigned> vacant;
	std::queue<Command> ready;

	std::condition_variable cond;
	std::mutex lock;
	bool dead = false;

	void join();

	unsigned displayed = 0;

	void threadEntry();

	void dump(const Command &cmd, unsigned sequenceCount);
};
}

#endif
