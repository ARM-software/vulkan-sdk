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

#include "asset_manager.hpp"
#include <stdio.h>

using namespace std;

namespace MaliSDK
{
Result AssetManager::readBinaryFile(const char *pPath, void **pData, size_t *pSize)
{
	FILE *file = fopen(pPath, "rb");
	if (!file)
		return RESULT_ERROR_IO;

	fseek(file, 0, SEEK_END);
	long len = ftell(file);
	rewind(file);

	*pData = malloc(len);
	if (!*pData)
	{
		fclose(file);
		return RESULT_ERROR_OUT_OF_MEMORY;
	}

	*pSize = len;
	if (fread(*pData, 1, *pSize, file) != *pSize)
	{
		free(pData);
		fclose(file);
		return RESULT_ERROR_IO;
	}

	fclose(file);
	return RESULT_SUCCESS;
}
}
