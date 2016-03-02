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

#ifndef FRAMEWORK_FENCE_MANAGER_HPP
#define FRAMEWORK_FENCE_MANAGER_HPP

#include "framework/common.hpp"
#include <vector>

namespace MaliSDK
{
/// @brief The FenceManager keeps track of fences which in turn are used to keep
/// track of GPU progress.
///
/// Whenever we submit work to the GPU, it is our responsibility to make sure
/// that the GPU is done
/// using our resources before we modify or delete them.
/// To implement this, we therefore use VkFences to keep track of all
/// vkQueueSubmits.
class FenceManager
{
public:
	/// @brief Constructor
	/// @param device The Vulkan device
	FenceManager(VkDevice device);

	/// @brief Destructor
	~FenceManager();

	/// @brief Begins the frame. Waits for GPU to trigger all outstanding fences.
	/// After begin frame returns, it is safe to reuse or delete resources which
	/// were used previously.
	///
	/// We wait for fences which completes N frames earlier, so we do not stall,
	/// waiting
	/// for all GPU work to complete before this returns.
	void beginFrame();

	/// @brief Called internally by the Context whenever submissions to GPU
	/// happens.
	VkFence requestClearedFence();

	/// @brief Gets the number of fences which are inFlight on the GPU.
	/// @returns The number of fences which can be waited for.
	unsigned getActiveFenceCount() const
	{
		return count;
	}

	/// @brief Gets an array for the fences which are inFlight on the GPU.
	/// @returns Array of waitable fences. Call @ref getActiveFenceCount for the
	/// number of fences.
	VkFence *getActiveFences()
	{
		return fences.data();
	}

private:
	VkDevice device = VK_NULL_HANDLE;
	std::vector<VkFence> fences;
	unsigned count = 0;
};
}

#endif
