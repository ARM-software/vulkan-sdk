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

#include "semaphore_manager.hpp"

namespace MaliSDK
{
SemaphoreManager::SemaphoreManager(VkDevice vkDevice)
    : device(vkDevice)
{
}

SemaphoreManager::~SemaphoreManager()
{
	for (auto &semaphore : recycledSemaphores)
		vkDestroySemaphore(device, semaphore, nullptr);
}

void SemaphoreManager::addClearedSemaphore(VkSemaphore semaphore)
{
	recycledSemaphores.push_back(semaphore);
}

VkSemaphore SemaphoreManager::getClearedSemaphore()
{
	if (recycledSemaphores.empty())
	{
		VkSemaphoreCreateInfo info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VkSemaphore semaphore;
		VK_CHECK(vkCreateSemaphore(device, &info, nullptr, &semaphore));
		return semaphore;
	}
	else
	{
		auto semaphore = recycledSemaphores.back();
		recycledSemaphores.pop_back();
		return semaphore;
	}
}
}
