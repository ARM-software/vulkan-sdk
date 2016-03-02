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

#ifndef FRAMEWORK_COMMAND_BUFFER_MANAGER_HPP
#define FRAMEWORK_COMMAND_BUFFER_MANAGER_HPP

#include "common.hpp"
#include <vector>

namespace MaliSDK
{
/// @brief The command buffer allocates command buffers and recycles them for
/// us.
/// This gives us a convenient interface where we can request command buffers
/// for use when rendering.
/// The manager is not thread-safe and for rendering in multiple threads,
/// multiple per-thread managers
/// should be used.
class CommandBufferManager
{
public:
	/// @brief Constructor
	/// @param device             The Vulkan device
	/// @param bufferLevel        The command buffer level to use,
	///                           either `VK_COMMAND_BUFFER_LEVEL_PRIMARY` or
	///                           `VK_COMMAND_BUFFER_LEVEL_SECONDARY`.
	/// @param graphicsQueueIndex The Vulkan queue family index for where we can
	/// submit graphics work.
	CommandBufferManager(VkDevice device, VkCommandBufferLevel bufferLevel, unsigned graphicsQueueIndex);

	/// @brief Destructor
	~CommandBufferManager();

	/// @brief Requests a fresh or recycled command buffer which is in the reset
	/// state.
	VkCommandBuffer requestCommandBuffer();

	/// @brief Begins the frame. When this is called,
	/// all command buffers managed by this class are assumed to be recycleable.
	void beginFrame();

private:
	VkDevice device = VK_NULL_HANDLE;
	VkCommandPool pool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> buffers;
	VkCommandBufferLevel commandBufferLevel;
	unsigned count = 0;
};
}

#endif
