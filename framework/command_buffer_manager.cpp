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

#include "command_buffer_manager.hpp"

namespace MaliSDK
{
CommandBufferManager::CommandBufferManager(VkDevice vkDevice, VkCommandBufferLevel bufferLevel,
                                           unsigned graphicsQueueIndex)
    : device(vkDevice)
    , commandBufferLevel(bufferLevel)
    , count(0)
{
	VkCommandPoolCreateInfo info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	info.queueFamilyIndex = graphicsQueueIndex;
	info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	VK_CHECK(vkCreateCommandPool(device, &info, nullptr, &pool));
}

CommandBufferManager::~CommandBufferManager()
{
	if (!buffers.empty())
		vkFreeCommandBuffers(device, pool, buffers.size(), buffers.data());
	vkDestroyCommandPool(device, pool, nullptr);
}

void CommandBufferManager::beginFrame()
{
	count = 0;
	vkResetCommandPool(device, pool, 0);
}

VkCommandBuffer CommandBufferManager::requestCommandBuffer()
{
	// Either we recycle a previously allocated command buffer, or create a new
	// one.
	VkCommandBuffer ret = VK_NULL_HANDLE;
	if (count < buffers.size())
	{
		ret = buffers[count++];
	}
	else
	{
		VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		info.commandPool = pool;
		info.level = commandBufferLevel;
		info.commandBufferCount = 1;
		VK_CHECK(vkAllocateCommandBuffers(device, &info, &ret));
		buffers.push_back(ret);

		count++;
	}

	return ret;
}
}
