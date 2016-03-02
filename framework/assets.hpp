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

#ifndef FRAMEWORK_ASSETS_HPP
#define FRAMEWORK_ASSETS_HPP

#include "common.hpp"
#include "libvulkan-stub.h"
#include <stdint.h>
#include <vector>

namespace MaliSDK
{
/// @brief Loads a SPIR-V shader module from assets.
/// @param device The Vulkan device.
/// @param path Path to the SPIR-V shader.
/// @returns A newly allocated shader module or VK_NULL_HANDLE on error.
VkShaderModule loadShaderModule(VkDevice device, const char *pPath);

/// @brief Loads texture data from assets.
///
/// @param      pPath Path to texture.
/// @param[out] pBuffer Output buffer where VK_FORMAT_R8G8B8A8_UNORM is placed.
/// @param[out] pWidth Width of the loaded texture.
/// @param[out] pHeight Height of the loaded texture.
///
/// @returns Error code.
Result loadRgba8888TextureFromAsset(const char *pPath, std::vector<uint8_t> *pBuffer, unsigned *pWidth,
                                    unsigned *pHeight);

/// @brief Loads an ASTC texture from assets.
///
/// Loads files created by astcenc tool.
///
/// @param pPath Path to texture.
/// @param[out] pBuffer Output buffer where an ASTC payload is placed.
/// @param[out] pWidth Width of the loaded texture.
/// @param[out] pHeight Height of the loaded texture.
/// @param[out] pFormat The format of the loaded texture.
Result loadASTCTextureFromAsset(const char *pPath, std::vector<uint8_t> *pBuffer, unsigned *pWidth, unsigned *pHeight,
                                VkFormat *pFormat);
}

#endif
