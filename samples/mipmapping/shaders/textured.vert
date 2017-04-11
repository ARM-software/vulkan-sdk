#version 310 es
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

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TexCoord;

layout(location = 0) out highp vec2 vTexCoord;
layout(location = 1) flat out int textureIndex;
layout(location = 2) flat out int highlightSize;
layout(location = 3) flat out int fixedMipLevel;

layout(set = 0, binding = 2, std140) uniform UBO
{
	mat4 MVP;
	int highlightedQuad;
	int mipmapType;
};

void main()
{
    gl_Position = MVP * vec4(Position, 0.0, 1.0);
    vTexCoord = TexCoord;

	if (gl_VertexIndex / 4 == highlightedQuad)
	{
		highlightSize = highlightedQuad + 1;
	}
	else
	{
		highlightSize = 0;
	}

	if (gl_VertexIndex >= 40)
	{
		fixedMipLevel = highlightedQuad;
	}
	else
	{
		fixedMipLevel = -1;
	}

	if (gl_VertexIndex >= 48)
	{
		textureIndex = 1;
		vTexCoord.y = (10.0 + vTexCoord.y + float(mipmapType)) / 12.0;
	}
	else if (gl_VertexIndex >= 44)
	{
		textureIndex = 1;
		vTexCoord.y = (vTexCoord.y + float(highlightedQuad)) / 12.0;
	}
	else
	{
		textureIndex = 0;
	}
}
