#version 310 es
/* Copyright (c) 2017, ARM Limited and Contributors
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

	// Set the highlighted status.
	if (gl_VertexIndex / 4 == highlightedQuad)
	{
		// If the vertex is part of the currently highlighted quad,
		// set its highlightSize to a non-zero value.
		// We also exploit the specific value of this parameter to
		// pass some information about the size of the quad to the
		// fragment shader: the highlight will need to be larger
		// for smaller quads, in proportion.
		highlightSize = highlightedQuad + 1;
	}
	else
	{
		// No highlight.
		highlightSize = 0;
	}

	// Set the mip level.
	if (gl_VertexIndex >= 40)
	{
		// The large, fixed size quad is used to showcase the
		// stretching of smaller mip layers, so we'll fix its
		// mip level to the one of the currently highlighted quad.
		fixedMipLevel = highlightedQuad;
	}
	else
	{
		// Don't set any mip level.
		fixedMipLevel = -1;
	}

	// Set the texture index and coordinates:
	// - index 0: the main texture;
	// - index 1: the auxiliary texture for labels.
	//
	// The auxiliary texture is split in 12 horizontal sections,
	// the first 10 corresponding to the quad sizes
	// and the last 2 corresponding to the mipmap types.
	if (gl_VertexIndex >= 48)
	{
		// Set the auxiliary texture and rescale the y coordinate to
		// pick the current mipmap type.
		textureIndex = 1;
		vTexCoord.y = (10.0 + vTexCoord.y + float(mipmapType)) / 12.0;
	}
	else if (gl_VertexIndex >= 44)
	{
		// Set the auxiliary texture and rescale the y coordinate to
		// pick the currently highlighted quad size.
		textureIndex = 1;
		vTexCoord.y = (vTexCoord.y + float(highlightedQuad)) / 12.0;
	}
	else
	{
		// Set the main texture.
		textureIndex = 0;
	}
}
