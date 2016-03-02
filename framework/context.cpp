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

#include "context.hpp"
#include "platform/platform.hpp"

namespace MaliSDK
{
Context::PerFrame::PerFrame(VkDevice device, unsigned graphicsQueueIndex)
    : device(device)
    , fenceManager(device)
    , commandManager(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, graphicsQueueIndex)
    , queueIndex(graphicsQueueIndex)
{
}

void Context::PerFrame::setSecondaryCommandManagersCount(unsigned count)
{
	secondaryCommandManagers.clear();
	for (unsigned i = 0; i < count; i++)
	{
		secondaryCommandManagers.emplace_back(
		    new CommandBufferManager(device, VK_COMMAND_BUFFER_LEVEL_SECONDARY, queueIndex));
	}
}

VkPhysicalDevice Context::getPhysicalDevice() const
{
	return pPlatform->getPhysicalDevice();
}

VkSemaphore Context::PerFrame::setSwapchainAcquireSemaphore(VkSemaphore acquireSemaphore)
{
	VkSemaphore ret = swapchainAcquireSemaphore;
	swapchainAcquireSemaphore = acquireSemaphore;
	return ret;
}

void Context::PerFrame::setSwapchainReleaseSemaphore(VkSemaphore releaseSemaphore)
{
	if (swapchainReleaseSemaphore != VK_NULL_HANDLE)
		vkDestroySemaphore(device, swapchainReleaseSemaphore, nullptr);
	swapchainReleaseSemaphore = releaseSemaphore;
}

void Context::PerFrame::beginFrame()
{
	fenceManager.beginFrame();
	commandManager.beginFrame();
	for (auto &pManager : secondaryCommandManagers)
		pManager->beginFrame();
}

Context::PerFrame::~PerFrame()
{
	if (swapchainAcquireSemaphore != VK_NULL_HANDLE)
		vkDestroySemaphore(device, swapchainAcquireSemaphore, nullptr);
	if (swapchainReleaseSemaphore != VK_NULL_HANDLE)
		vkDestroySemaphore(device, swapchainReleaseSemaphore, nullptr);
}

void Context::waitIdle()
{
	vkDeviceWaitIdle(device);
}

Result Context::onPlatformUpdate(Platform *pPlatform)
{
	device = pPlatform->getDevice();
	queue = pPlatform->getGraphicsQueue();
	this->pPlatform = pPlatform;

	waitIdle();

	// Initialize per-frame resources.
	// Every swapchain image has its own command pool and fence manager.
	// This makes it very easy to keep track of when we can reset command buffers
	// and such.
	perFrame.clear();
	for (unsigned i = 0; i < pPlatform->getNumSwapchainImages(); i++)
		perFrame.emplace_back(new PerFrame(device, pPlatform->getGraphicsQueueIndex()));

	setRenderingThreadCount(renderingThreadCount);

	return RESULT_SUCCESS;
}

void Context::submit(VkCommandBuffer cmd)
{
	submitCommandBuffer(cmd, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void Context::submitSwapchain(VkCommandBuffer cmd)
{
	// For the first frames, we will create a release semaphore.
	// This can be reused every frame. Semaphores are reset when they have been
	// successfully been waited on.
	// If we aren't using acquire semaphores, we aren't using release semaphores
	// either.
	if (getSwapchainReleaseSemaphore() == VK_NULL_HANDLE && getSwapchainAcquireSemaphore() != VK_NULL_HANDLE)
	{
		VkSemaphore releaseSemaphore;
		VkSemaphoreCreateInfo semaphoreInfo = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
		VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &releaseSemaphore));
		perFrame[swapchainIndex]->setSwapchainReleaseSemaphore(releaseSemaphore);
	}

	submitCommandBuffer(cmd, getSwapchainAcquireSemaphore(), getSwapchainReleaseSemaphore());
}

void Context::submitCommandBuffer(VkCommandBuffer cmd, VkSemaphore acquireSemaphore, VkSemaphore releaseSemaphore)
{
	// All queue submissions get a fence that CPU will wait
	// on for synchronization purposes.
	VkFence fence = getFenceManager().requestClearedFence();

	VkSubmitInfo info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	info.commandBufferCount = 1;
	info.pCommandBuffers = &cmd;

	const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	info.waitSemaphoreCount = acquireSemaphore != VK_NULL_HANDLE ? 1 : 0;
	info.pWaitSemaphores = &acquireSemaphore;
	info.pWaitDstStageMask = &waitStage;
	info.signalSemaphoreCount = releaseSemaphore != VK_NULL_HANDLE ? 1 : 0;
	info.pSignalSemaphores = &releaseSemaphore;

	VK_CHECK(vkQueueSubmit(queue, 1, &info, fence));
}
}
