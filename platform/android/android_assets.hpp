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

#ifndef PLATFORM_ANDROID_ASSETS
#define PLATFORM_ANDROID_ASSETS

#include "../asset_manager.hpp"
#include <android/asset_manager.h>
#include <string>

namespace MaliSDK
{

/// @brief An asset manager implementation for Android. Uses AAssetManager to
/// load assets.
class AndroidAssetManager : public AssetManager
{
public:
	/// @brief Reads a binary file as a raw blob.
	/// @param pPath The path of the asset.
	/// @param[out] ppData allocated output data. Must be freed with `free()`.
	/// @param[out] pSize The size of the allocated data.
	/// @returns Error code
	virtual Result readBinaryFile(const char *pPath, void **ppData, size_t *pSize) override;

	/// @brief Sets the asset manager to use. Called from platform.
	/// @param pAssetManager The asset manager.
	void setAssetManager(AAssetManager *pAssetManager)
	{
		pManager = pAssetManager;
	}

private:
	AAssetManager *pManager = nullptr;
};
}

#endif
