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

#ifndef FRAMEWORK_APPLICATION_HPP
#define FRAMEWORK_APPLICATION_HPP

#include "platform/platform.hpp"

namespace MaliSDK
{
class Context;

/// @brief VulkanApplication is inherited by all samples.
/// The common platform code will run the main loop and take care of application
/// lifecycle.
class VulkanApplication
{
public:
	/// @brief Destructor
	virtual ~VulkanApplication() = default;

	/// @brief This function is called when the context is brought up and is
	/// the constructor of this class.
	///
	/// @param pContext The Vulkan context
	/// @returns true if initialization succeeded. If initialization fails, the
	/// application will terminate.
	virtual bool initialize(Context *pContext) = 0;

	/// @brief Called when the swapchain has been initialized.
	///
	/// updateSwapchain is always called after the first initialize.
	///
	/// If the swapchain for some reason is lost or recreated,
	/// this can be called several times during the lifetime of the application.
	///
	/// @param backbuffers A vector containing all the backbuffers in the current
	/// swapchain
	/// @param dimensions The dimensions of the swapchain, along with the VkFormat
	/// needed to create render passes
	virtual void updateSwapchain(const std::vector<VkImage> &backbuffers,
	                             const Platform::SwapchainDimensions &dimensions) = 0;

	/// @brief Render a frame
	///
	/// @param swapchainIndex  The swapchain index to render into, previously
	/// obtained by backbuffers
	///                        parameter in updateSwapchain.
	/// @param deltaTime       The time in seconds since last call to render.
	virtual void render(unsigned swapchainIndex, float deltaTime) = 0;

	/// @brief Destructor for the class.
	///
	/// After this call returns, initialize can be called again.
	virtual void terminate() = 0;
};

/// @brief Creates a new application instance
///
/// This function must be implemented by all samples.
/// The function is called from the common platform code.
/// Each sample will live in its own binary, so there is no need to consider
/// symbol clashing.
/// \code
/// VulkanApplication *MaliSDK::createApplication()
/// {
///     return new MyApplication;
/// }
/// \endcode
VulkanApplication *createApplication();
}

#endif
