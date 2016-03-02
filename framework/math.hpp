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

#ifndef FRAMEWORK_MATH_HPP
#define FRAMEWORK_MATH_HPP

#define GLM_FORCE_SWIZZLE
#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform.hpp>

namespace MaliSDK
{
/// @brief Converts an OpenGL style projection matrix to Vulkan style projection
/// matrix.
///
/// Vulkan has a topLeft clipSpace with [0, 1] depth range instead of [-1, 1].
///
/// GLM outputs projection matrices in GL style clipSpace,
/// perform a simple fix-up step to change the projection to VulkanStyle.
///
/// @param proj Projection matrix
///
/// @returns A Vulkan compatible projection matrix.
inline glm::mat4 vulkanStyleProjection(const glm::mat4 &proj)
{
	using namespace glm;

	// Flip Y in clipspace. X = -1, Y = -1 is topLeft in Vulkan.
	auto mat = scale(mat4(1.0f), vec3(1.0f, -1.0f, 1.0f));

	// Z depth is [0, 1] range instead of [-1, 1].
	mat = scale(mat, vec3(1.0f, 1.0f, 0.5f));
	return translate(mat, vec3(0.0f, 0.0f, 1.0f)) * proj;
}
}

#endif
