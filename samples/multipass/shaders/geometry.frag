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

precision mediump float;

layout(location = 0) in mediump vec2 vTexCoord;
layout(location = 1) in mediump vec3 vNormal;

// glslc complains about too few render targets, this is a workaround.
layout(location = 0) out vec4 outputs[3];

layout(set = 0, binding = 0) uniform sampler2D tex;

void main()
{
	vec3 albedo = texture(tex, vTexCoord).rgb;

	// Emissive. Make the ARM logo emissive blue.
	vec3 graydiff = albedo - dot(albedo, vec3(0.29, 0.60, 0.11));
	float emissive = 3.0 * smoothstep(0.02, 0.1, dot(graydiff, graydiff));
	outputs[0] = vec4(emissive * albedo, 1.0);

	// Albedo
	outputs[1] = vec4(albedo, 1.0);
	// Normals, pack -1, +1 range to 0, 1.
	outputs[2] = vec4(0.5 * normalize(vNormal) + 0.5, 1.0);
}

