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

layout(location = 0) out vec4 outLight;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform mediump subpassInput albedo;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform highp subpassInput depth;
layout(input_attachment_index = 2, set = 0, binding = 2) uniform mediump subpassInput normals;

layout(push_constant, std430) uniform Registers
{
	mat4 inv_view_proj;
	vec4 color;
	vec4 position;
	vec2 inv_resolution;
} registers;

void main()
{
	// Load G-Buffer albedo from tile buffer.
	vec4 color = subpassLoad(albedo);

	// Load depth from tile buffer and reconstruct world position.
	vec4 clip = vec4(gl_FragCoord.xy * registers.inv_resolution * 2.0 - 1.0, subpassLoad(depth).x, 1.0);
	highp vec4 world_w = registers.inv_view_proj * clip;
	highp vec3 world = world_w.xyz / world_w.w;

	// Load normal from tile buffer.
	vec3 normal = 2.0 * subpassLoad(normals).xyz - 1.0;

	// Basic point light shading.
	vec3 world_to_light = registers.position.xyz - world;
	float dist = length(world_to_light);
	float atten = 1.0 - smoothstep(0.5 * registers.position.w, registers.position.w, dist);
	world_to_light = normalize(world_to_light);
	float ndotl = clamp(dot(normal, world_to_light), 0.0, 1.0);

	outLight = ndotl * atten * registers.color * color;
}
