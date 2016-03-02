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

#ifndef FRAMEWORK_SEMAPHORE_MANAGER_HPP
#define FRAMEWORK_SEMAPHORE_MANAGER_HPP

#include "framework/common.hpp"
#include <vector>

namespace MaliSDK
{
/// @brief The SemaphoreManager keeps track of semaphores
///
/// This class is mostly used by the WSI implementation so we can recycle
/// semaphores.
class SemaphoreManager
{
public:
	/// @brief Constructor
	/// @param device The Vulkan device
	SemaphoreManager(VkDevice device);

	/// @brief Destructor
	~SemaphoreManager();

	/// @brief Takes ownership of a recycled semaphore or creates a new one.
	/// @returns A VkSemaphore. This semaphore is owned by the API user and must
	/// be destroyed or given back to the semaphore manager.
	VkSemaphore getClearedSemaphore();

	/// @brief Gives ownership of a cleared semaphore to the semaphore manager.
	/// @param semaphore A cleared semaphore. The API user relinquishes ownership
	/// of the semaphore.
	void addClearedSemaphore(VkSemaphore semaphore);

private:
	VkDevice device = VK_NULL_HANDLE;
	std::vector<VkSemaphore> recycledSemaphores;
};
}

#endif
