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

#include "android_assets.hpp"

using namespace std;

namespace MaliSDK
{
Result AndroidAssetManager::readBinaryFile(const char *pPath, void **ppData, size_t *pSize)
{
	if (!pManager)
	{
		LOGE("Asset manager does not exist.");
		return RESULT_ERROR_GENERIC;
	}

	AAsset *asset = AAssetManager_open(pManager, pPath, AASSET_MODE_BUFFER);
	if (!asset)
	{
		LOGE("AAssetManager_open() failed to load file: %s.", pPath);
		return RESULT_ERROR_IO;
	}

	const void *buffer = AAsset_getBuffer(asset);
	if (buffer)
	{
		*pSize = AAsset_getLength(asset);
		*ppData = malloc(*pSize);
		if (!*ppData)
		{
			LOGE("Failed to allocate buffer for asset: %s.", pPath);
			AAsset_close(asset);
			return RESULT_ERROR_OUT_OF_MEMORY;
		}

		memcpy(*ppData, buffer, *pSize);
		AAsset_close(asset);
		return RESULT_SUCCESS;
	}
	else
	{
		LOGE("Failed to obtain buffer for asset: %s.", pPath);
		AAsset_close(asset);
		return RESULT_ERROR_IO;
	}
}
}
