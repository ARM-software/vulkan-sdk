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

#include "fence_manager.hpp"

namespace MaliSDK
{
FenceManager::FenceManager(VkDevice vkDevice)
    : device(vkDevice)
    , count(0)
{
}

FenceManager::~FenceManager()
{
	beginFrame();
	for (auto &fence : fences)
		vkDestroyFence(device, fence, nullptr);
}

void FenceManager::beginFrame()
{
	// If we have outstanding fences for this swapchain image, wait for them to
	// complete first.
	// Normally, this doesn't really block at all,
	// since we're waiting for old frames to have been completed, but just in
	// case.
	if (count != 0)
	{
		vkWaitForFences(device, count, fences.data(), true, UINT64_MAX);
		vkResetFences(device, count, fences.data());
	}
	count = 0;
}

VkFence FenceManager::requestClearedFence()
{
	if (count < fences.size())
		return fences[count++];

	VkFence fence;
	VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	VK_CHECK(vkCreateFence(device, &info, nullptr, &fence));
	fences.push_back(fence);
	count++;
	return fence;
}
}
