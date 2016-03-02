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

#include "framework/application.hpp"
#include "framework/assets.hpp"
#include "framework/common.hpp"
#include "framework/context.hpp"
#include "framework/math.hpp"
#include "platform/platform.hpp"
#include <algorithm>

using namespace MaliSDK;
using namespace std;
using namespace glm;

#define NUM_INSTANCES_X 16
#define NUM_INSTANCES_Y 16
#define NUM_INSTANCES_Z 16

struct Backbuffer
{
	// We get this image from the platform. Its memory is bound to the display or window.
	VkImage image;

	// We need an image view to be able to access the image as a framebuffer.
	VkImageView view;

	// The actual framebuffer.
	VkFramebuffer framebuffer;
};

struct Buffer
{
	// The buffer object.
	VkBuffer buffer;

	// Buffer objects are backed by device memory.
	VkDeviceMemory memory;
};

struct Image
{
	// The image itself.
	VkImage image;

	// To use images in shaders, we need an image view.
	VkImageView view;

	// Memory for the image.
	VkDeviceMemory memory;
};

struct Texture
{
	// The image object itself.
	VkImage image;

	// To use images in shaders, we need an image view.
	VkImageView view;

	// Memory for the texture.
	VkDeviceMemory memory;

	// Images have layouts, stores the current layout used.
	VkImageLayout layout;

	// For simplicity, tie a sampler object to the texture.
	VkSampler sampler;

	unsigned width, height;
};

struct CubeVertex
{
	vec3 pos;
	vec3 normal;
	vec2 tex;
};

struct LightingData
{
	mat4 invViewProj;
	vec4 color;
	vec4 position;
	vec2 invResolution;
};

static inline unsigned numMiplevels(unsigned width, unsigned height)
{
	unsigned levels = 0;
	while (width || height)
	{
		width >>= 1;
		height >>= 1;
		levels++;
	}
	return levels;
}

class Multipass : public VulkanApplication
{
public:
	virtual bool initialize(Context *pContext);
	virtual void updateSwapchain(const std::vector<VkImage> &backbuffers,
	                             const Platform::SwapchainDimensions &dimensions);
	virtual void render(unsigned swapchainIndex, float deltaTime);
	virtual void terminate();

private:
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;

	Context *pContext;

	vector<Backbuffer> backbuffers;
	unsigned width, height;

	// The resources.
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSets[3];

	// The renderpass description.
	VkRenderPass renderPass;

	// The graphics pipelines.
	VkPipelineCache pipelineCache;
	VkPipeline pipeline;
	VkPipeline lightPipeline;
	VkPipeline lightPipelineInside;
	VkPipeline debugPipeline;

	// Pipeline layout for resources.
	VkPipelineLayout pipelineLayoutGBuffer;
	VkPipelineLayout pipelineLayoutLighting;

	VkDescriptorSetLayout setLayouts[3];

	// Buffer that holds the vertices.
	Buffer vertexBuffer;
	// Buffer that holds the indices.
	Buffer indexBuffer;
	// Buffer that holds per-cube data.
	Buffer perInstanceBuffer;

	// A uniform that holds one MVP.
	Buffer uniformBuffer = {};
	// A permanently mapped pointer to uniformBuffer.
	uint8_t *uboData = nullptr;
	VkDeviceSize uboAlignment = 0;

	// Buffer that holds the vertices for the quad.
	Buffer quadVertexBuffer;

	Texture texture;

	// Image for the depth buffer.
	Image depthImage;
	// A depth-only view for multipass.
	VkImageView depthImageDepthOnlyView;

	// Image for the normals.
	Image normalImage;

	// Image for the albedo values.
	Image albedoImage;

	Buffer createBuffer(const void *data, size_t size, VkFlags usage);
	Texture createTexture(const char *pPath);
	void createDescriptors();

	uint32_t findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements);
	uint32_t findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements, uint32_t hostRequirements);

	void createRenderPass(VkFormat format);
	void termBackbuffers();

	void initBuffers();
	void createLightPipeline();
	void createDebugPipeline();
	void createGBufferPipeline();
	void createPipelineLayout();

	void imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
	                        VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
	                        VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout, VkImageLayout newLayout);
	void imageMemoryBarrierLevel(VkCommandBuffer cmd, VkImage image, unsigned level, VkAccessFlags srcAccessMask,
	                             VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
	                             VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout, VkImageLayout newLayout);
	Image createImage(VkFlags usage, VkFormat format, VkImageAspectFlags aspectMask, unsigned width, unsigned height,
	                  unsigned mipLevels = 1);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectMask);

	float totalTime = 0.0f;
};

// To create a buffer, both the device and application have requirements from the buffer object.
// Vulkan exposes the different types of buffers the device can allocate, and we have to find a suitable one.
// deviceRequirements is a bitmask expressing which memory types can be used for a buffer object.
// The different memory types' properties must match with what the application wants.
uint32_t Multipass::findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements)
{
	const VkPhysicalDeviceMemoryProperties &props = pContext->getPlatform().getMemoryProperties();
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if (deviceRequirements & (1u << i))
		{
			if ((props.memoryTypes[i].propertyFlags & hostRequirements) == hostRequirements)
			{
				return i;
			}
		}
	}

	LOGE("Failed to obtain suitable memory type.\n");
	abort();
}

uint32_t Multipass::findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements, uint32_t hostRequirements)
{
	const VkPhysicalDeviceMemoryProperties &props = pContext->getPlatform().getMemoryProperties();
	for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
	{
		if (deviceRequirements & (1u << i))
		{
			if ((props.memoryTypes[i].propertyFlags & hostRequirements) == hostRequirements)
			{
				return i;
			}
		}
	}

	// On desktop systems, we'll need a fallback to plain device local.
	if (hostRequirements & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
		return findMemoryTypeFromRequirementsWithFallback(deviceRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// If we cannot find the particular memory type we're looking for, just pick the first one available.
	if (hostRequirements != 0)
		return findMemoryTypeFromRequirements(deviceRequirements, 0);
	else
	{
		LOGE("Failed to obtain suitable memory type.\n");
		abort();
	}
}

Buffer Multipass::createBuffer(const void *data, size_t size, VkFlags usage)
{
	Buffer buffer;
	VkDevice device = pContext->getDevice();

	VkBufferCreateInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bufferInfo.size = size;
	bufferInfo.usage = usage;

	VK_CHECK(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer.buffer));

	// Ask device about its memory requirements.
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, buffer.buffer, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memoryAllocateInfo.allocationSize = memoryRequirements.size;

	// We want host visible and coherent memory to simplify things.
	memoryAllocateInfo.memoryTypeIndex = findMemoryTypeFromRequirements(
	    memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VK_CHECK(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &buffer.memory));

	// Buffers are not backed by memory, so bind our memory explicitly to the buffer.
	vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);

	// Map the memory and dump data in there.
	if (data)
	{
		void *memoryPointer;
		VK_CHECK(vkMapMemory(device, buffer.memory, 0, size, 0, &memoryPointer));

		memcpy(memoryPointer, data, size);
		vkUnmapMemory(device, buffer.memory);
	}

	return buffer;
}

void Multipass::initBuffers()
{
	static const CubeVertex vertices[] = {
		// Front face
		{ vec3(-1.0f, -1.0f, +1.0f), vec3(0.0f, 0.0f, +1.0f), vec2(0.0f, 1.0f) },
		{ vec3(+1.0f, -1.0f, +1.0f), vec3(0.0f, 0.0f, +1.0f), vec2(1.0f, 1.0f) },
		{ vec3(-1.0f, +1.0f, +1.0f), vec3(0.0f, 0.0f, +1.0f), vec2(0.0f, 0.0f) },
		{ vec3(+1.0f, +1.0f, +1.0f), vec3(0.0f, 0.0f, +1.0f), vec2(1.0f, 0.0f) },

		// Back face
		{ vec3(+1.0f, -1.0f, -1.0f), vec3(0.0f, 0.0f, -1.0f), vec2(0.0f, 1.0f) },
		{ vec3(-1.0f, -1.0f, -1.0f), vec3(0.0f, 0.0f, -1.0f), vec2(1.0f, 1.0f) },
		{ vec3(+1.0f, +1.0f, -1.0f), vec3(0.0f, 0.0f, -1.0f), vec2(0.0f, 0.0f) },
		{ vec3(-1.0f, +1.0f, -1.0f), vec3(0.0f, 0.0f, -1.0f), vec2(1.0f, 0.0f) },

		// Left face
		{ vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, 0.0f, 0.0f), vec2(0.0f, 1.0f) },
		{ vec3(-1.0f, -1.0f, +1.0f), vec3(-1.0f, 0.0f, 0.0f), vec2(1.0f, 1.0f) },
		{ vec3(-1.0f, +1.0f, -1.0f), vec3(-1.0f, 0.0f, 0.0f), vec2(0.0f, 0.0f) },
		{ vec3(-1.0f, +1.0f, +1.0f), vec3(-1.0f, 0.0f, 0.0f), vec2(1.0f, 0.0f) },

		// Right face
		{ vec3(+1.0f, -1.0f, +1.0f), vec3(+1.0f, 0.0f, 0.0f), vec2(0.0f, 1.0f) },
		{ vec3(+1.0f, -1.0f, -1.0f), vec3(+1.0f, 0.0f, 0.0f), vec2(1.0f, 1.0f) },
		{ vec3(+1.0f, +1.0f, +1.0f), vec3(+1.0f, 0.0f, 0.0f), vec2(0.0f, 0.0f) },
		{ vec3(+1.0f, +1.0f, -1.0f), vec3(+1.0f, 0.0f, 0.0f), vec2(1.0f, 0.0f) },

		// Top face
		{ vec3(-1.0f, +1.0f, +1.0f), vec3(0.0f, +1.0f, 0.0f), vec2(0.0f, 1.0f) },
		{ vec3(+1.0f, +1.0f, +1.0f), vec3(0.0f, +1.0f, 0.0f), vec2(1.0f, 1.0f) },
		{ vec3(-1.0f, +1.0f, -1.0f), vec3(0.0f, +1.0f, 0.0f), vec2(0.0f, 0.0f) },
		{ vec3(+1.0f, +1.0f, -1.0f), vec3(0.0f, +1.0f, 0.0f), vec2(1.0f, 0.0f) },

		// Bottom face
		{ vec3(-1.0f, -1.0f, -1.0f), vec3(0.0f, -1.0f, 0.0f), vec2(0.0f, 1.0f) },
		{ vec3(+1.0f, -1.0f, -1.0f), vec3(0.0f, -1.0f, 0.0f), vec2(1.0f, 1.0f) },
		{ vec3(-1.0f, -1.0f, +1.0f), vec3(0.0f, -1.0f, 0.0f), vec2(0.0f, 0.0f) },
		{ vec3(+1.0f, -1.0f, +1.0f), vec3(0.0f, -1.0f, 0.0f), vec2(1.0f, 0.0f) },
	};

	static const uint16_t indices[] = {
		0,  1,  2,  3,  2,  1,  4,  5,  6,  7,  6,  5,  8,  9,  10, 11, 10, 9,
		12, 13, 14, 15, 14, 13, 16, 17, 18, 19, 18, 17, 20, 21, 22, 23, 22, 21,
	};

	vector<vec4> perInstance;
	perInstance.reserve(NUM_INSTANCES_X * NUM_INSTANCES_Y * NUM_INSTANCES_Z);
	for (int z = 0; z < NUM_INSTANCES_Z; z++)
	{
		for (int y = 0; y < NUM_INSTANCES_Y; y++)
		{
			for (int x = 0; x < NUM_INSTANCES_X; x++)
			{
				perInstance.push_back(vec4(2.0f + 4.0f * (x - NUM_INSTANCES_X / 2),
				                           2.0f + 4.0f * (y - NUM_INSTANCES_Y / 2),
				                           2.0f + 4.0f * (z - NUM_INSTANCES_Z / 2), 0.0f));
			}
		}
	}

	// Sort front to back. Cubes closer to origin are rendered first.
	sort(begin(perInstance), end(perInstance), [](const vec4 &a, const vec4 &b) { return dot(a, a) < dot(b, b); });

	vertexBuffer = createBuffer(vertices, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	indexBuffer = createBuffer(indices, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
	perInstanceBuffer =
	    createBuffer(perInstance.data(), sizeof(vec4) * perInstance.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	const vec2 quadVertices[] = {
		vec2(-1.0f, -1.0f), vec2(-1.0f, 1.0f), vec2(1.0f, -1.0f), vec2(1.0f, 1.0f),
	};

	quadVertexBuffer = createBuffer(quadVertices, sizeof(quadVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void Multipass::imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
                                   VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
                                   VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, false, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Multipass::imageMemoryBarrierLevel(VkCommandBuffer cmd, VkImage image, unsigned level, VkAccessFlags srcAccessMask,
                                        VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
                                        VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout,
                                        VkImageLayout newLayout)
{
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = level;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, false, 0, nullptr, 0, nullptr, 1, &barrier);
}

Texture Multipass::createTexture(const char *pPath)
{
	// We want to first create a staging buffer.
	//
	// We will then copy this buffer into an optimally tiled texture with vkCmdCopyBufferToImage.
	// The layout of such a texture is not specified as it is highly GPU-dependent and optimized for
	// utiliving texture caches better.
	unsigned width, height;
	vector<uint8_t> buffer;

	if (FAILED(loadRgba8888TextureFromAsset(pPath, &buffer, &width, &height)))
	{
		LOGE("Failed to load texture from asset.\n");
		abort();
	}

	VkDevice device = pContext->getDevice();

	Buffer stagingBuffer = createBuffer(buffer.data(), width * height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	unsigned numLevels = numMiplevels(width, height);
	Image textureImage =
	    createImage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
	                VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, width, height, numLevels);

	// We need to transfer the staging texture into the real texture.
	// For this we will need a command buffer.
	VkCommandBuffer commandBuffer = pContext->requestPrimaryCommandBuffer();

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	// Transition the uninitialized texture into a TRANSFER_DST_OPTIMAL layout.
	// We do not need to wait for anything to make the transition, so use TOP_OF_PIPE_BIT as the srcStageMask.
	imageMemoryBarrier(commandBuffer, textureImage.image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
	                   VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
	                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkBufferImageCopy region;
	memset(&region, 0, sizeof(region));
	region.bufferOffset = 0;
	region.bufferRowLength = width;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = width;
	region.imageExtent.height = height;
	region.imageExtent.depth = 1;

	// Copy the buffer to our optimally tiled image.
	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.buffer, textureImage.image,
	                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	// Begin mipmapping. First transition level 0 into a TRANSFER_SRC layout.
	imageMemoryBarrierLevel(commandBuffer, textureImage.image, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
	                        VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
	                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

	// We can mip-map images in Vulkan using vkCmdBlitImage. There is no vkCmdGenerateMipmap or anything like that.
	for (unsigned i = 1; i < numLevels; i++)
	{
		// Set up a blit from previous mip-level to the next.
		VkImageBlit region = {};
		region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.srcSubresource.mipLevel = i - 1;
		region.srcSubresource.baseArrayLayer = 0;
		region.srcSubresource.layerCount = 1;
		region.dstSubresource = region.srcSubresource;
		region.dstSubresource.mipLevel = i;
		region.srcOffsets[1] = { std::max(int(width >> (i - 1)), 1), std::max(int(height >> (i - 1)), 1), 1 };
		region.dstOffsets[1] = { std::max(int(width >> i), 1), std::max(int(height >> i), 1), 1 };

		vkCmdBlitImage(commandBuffer, textureImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureImage.image,
		               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

		// We're done transfering from this level now, transition it to SHADER_READ_ONLY.
		imageMemoryBarrierLevel(commandBuffer, textureImage.image, i - 1, VK_ACCESS_TRANSFER_READ_BIT,
		                        VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// If this is the last level, we can transition directly to SHADER_READ_ONLY.
		if (i + 1 == numLevels)
		{
			imageMemoryBarrierLevel(commandBuffer, textureImage.image, i, VK_ACCESS_TRANSFER_WRITE_BIT,
			                        VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		else
		{
			imageMemoryBarrierLevel(commandBuffer, textureImage.image, i, VK_ACCESS_TRANSFER_WRITE_BIT,
			                        VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		}
	}

	VK_CHECK(vkEndCommandBuffer(commandBuffer));
	pContext->submit(commandBuffer);

	// We want to free the staging buffer and memory right away, so wait for GPU to complete the transfer.
	vkQueueWaitIdle(pContext->getGraphicsQueue());

	// Now it's safe to free the temporary resources.
	vkFreeMemory(device, stagingBuffer.memory, nullptr);
	vkDestroyBuffer(device, stagingBuffer.buffer, nullptr);

	// Finally, create a sampler, use tri-linear filtering here for best quality.
	VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = false;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

	VkSampler sampler;
	VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));

	Texture ret = {
		textureImage.image,
		textureImage.view,
		textureImage.memory,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		sampler,
		width,
		height,
	};
	return ret;
}

VkImageView Multipass::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectMask)
{
	// Create an image view for the image.
	// Note that CreateImageView must happen after BindImageMemory.
	VkImageViewCreateInfo imageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = format;
	imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	imageViewInfo.subresourceRange.aspectMask = aspectMask;
	imageViewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	imageViewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageView view;
	VK_CHECK(vkCreateImageView(pContext->getDevice(), &imageViewInfo, nullptr, &view));
	return view;
}

Image Multipass::createImage(VkFlags usage, VkFormat format, VkImageAspectFlags aspectMask, unsigned width,
                             unsigned height, unsigned numLevels)
{
	VkDevice device = pContext->getDevice();

	Image image;

	// We will transition the actual image into a proper layout before transfering any data, so leave it as undefined.
	VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = format;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = numLevels;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = usage;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// Create image.
	VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &image.image));

	// Allocate memory for the texture.
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(device, image.image, &memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memoryAllocateInfo.allocationSize = memoryRequirements.size;

	if (usage & VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT)
	{
		// If an image is transient, try to find lazily allocated memory.
		memoryAllocateInfo.memoryTypeIndex = findMemoryTypeFromRequirementsWithFallback(
		    memoryRequirements.memoryTypeBits,
		    VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}
	else
	{
		// If a device local memory type exists, we should use that.
		memoryAllocateInfo.memoryTypeIndex = findMemoryTypeFromRequirementsWithFallback(
		    memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	}

	VK_CHECK(vkAllocateMemory(device, &memoryAllocateInfo, nullptr, &image.memory));
	VK_CHECK(vkBindImageMemory(device, image.image, image.memory, 0));

	image.view = createImageView(image.image, format, aspectMask);
	return image;
}

void Multipass::createPipelineLayout()
{
	VkDevice device = pContext->getDevice();

	VkDescriptorSetLayoutBinding bindings[5] = {};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo descriptorSetInfos[3] = {
		{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO },
	};
	descriptorSetInfos[0].bindingCount = 1;
	descriptorSetInfos[0].pBindings = bindings;

	VK_CHECK(vkCreateDescriptorSetLayout(device, descriptorSetInfos, nullptr, &setLayouts[0]));

	bindings[1].binding = 0;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[2].binding = 1;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[3].binding = 2;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	bindings[3].descriptorCount = 1;
	bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[4].binding = 0;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	bindings[4].descriptorCount = 1;
	bindings[4].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	descriptorSetInfos[1].bindingCount = 3;
	descriptorSetInfos[1].pBindings = &bindings[1];

	descriptorSetInfos[2].bindingCount = 1;
	descriptorSetInfos[2].pBindings = &bindings[4];

	VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetInfos[1], nullptr, &setLayouts[1]));
	VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorSetInfos[2], nullptr, &setLayouts[2]));

	// Setup layout for G-buffer pass.
	{
		// Setup the push constants. They are two mat4 in the vertex shader.
		VkPushConstantRange pushConstantRange = {};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(mat4) * 2;

		VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &setLayouts[0];
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayoutGBuffer));
	}

	// Setup layout for lighting pass.
	{
		VkPushConstantRange pushConstantRange = {};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(LightingData);

		VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
		layoutInfo.setLayoutCount = 2;
		layoutInfo.pSetLayouts = &setLayouts[1];
		layoutInfo.pushConstantRangeCount = 1;
		layoutInfo.pPushConstantRanges = &pushConstantRange;

		VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayoutLighting));
	}
}

void Multipass::createDescriptors()
{
	VkDevice device = pContext->getDevice();

	const VkDescriptorPoolSize poolSizes[3] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 3 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
	};

	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.poolSizeCount = 3;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 3;
	VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

	VkDescriptorSetAllocateInfo allocateInfo[3] = {
		{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO },
		{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO },
		{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO },
	};

	for (unsigned i = 0; i < 3; i++)
	{
		allocateInfo[i].descriptorPool = descriptorPool;
		allocateInfo[i].descriptorSetCount = 1;
		allocateInfo[i].pSetLayouts = &setLayouts[i];
		VK_CHECK(vkAllocateDescriptorSets(device, &allocateInfo[i], &descriptorSets[i]));
	}

	VkWriteDescriptorSet writes[5] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
	};

	VkDescriptorImageInfo imageInfo = { texture.sampler, texture.view, texture.layout };
	VkDescriptorBufferInfo uboInfo = { uniformBuffer.buffer, 0, sizeof(mat4) };

	writes[0].dstSet = descriptorSets[0];
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imageInfo;

	VkDescriptorImageInfo inputImageInfos[3] = {};
	inputImageInfos[0].imageView = albedoImage.view;
	inputImageInfos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputImageInfos[1].imageView = depthImageDepthOnlyView;
	inputImageInfos[1].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	inputImageInfos[2].imageView = normalImage.view;
	inputImageInfos[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	writes[1].dstSet = descriptorSets[1];
	writes[1].dstBinding = 0;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	writes[1].pImageInfo = inputImageInfos;

	writes[2].dstSet = descriptorSets[1];
	writes[2].dstBinding = 1;
	writes[2].descriptorCount = 1;
	writes[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	writes[2].pImageInfo = &inputImageInfos[1];

	writes[3].dstSet = descriptorSets[1];
	writes[3].dstBinding = 2;
	writes[3].descriptorCount = 1;
	writes[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	writes[3].pImageInfo = &inputImageInfos[2];

	writes[4].dstSet = descriptorSets[2];
	writes[4].dstBinding = 0;
	writes[4].descriptorCount = 1;
	writes[4].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	writes[4].pBufferInfo = &uboInfo;

	vkUpdateDescriptorSets(device, 5, writes, 0, nullptr);
}

bool Multipass::initialize(Context *pContext)
{
	this->pContext = pContext;

	// Create pipeline cache.
	VkPipelineCacheCreateInfo cacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	VK_CHECK(vkCreatePipelineCache(pContext->getDevice(), &cacheInfo, nullptr, &pipelineCache));

	// Create the buffers.
	initBuffers();

	// Initialize the pipeline layout.
	createPipelineLayout();

	// Load texture.
	texture = createTexture("textures/texture.png");

	// Find supported depth-stencil format.
	auto gpu = pContext->getPhysicalDevice();
	VkFormatProperties unormProperties, floatProperties;

	// One of these must be supported.
	vkGetPhysicalDeviceFormatProperties(gpu, VK_FORMAT_D24_UNORM_S8_UINT, &unormProperties);
	vkGetPhysicalDeviceFormatProperties(gpu, VK_FORMAT_D32_SFLOAT_S8_UINT, &floatProperties);

	if (unormProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
	else if (floatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
	else
		return false;

	return true;
}

void Multipass::createRenderPass(VkFormat format)
{
	VkAttachmentDescription attachmentDescriptions[4] = {};

	// Setup the color attachment.

	// Color backbuffer format.
	attachmentDescriptions[0].format = format;
	// Not multisampled.
	attachmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	// When starting the frame, we want tiles to be cleared.
	attachmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// When ending the frame, we want tiles to be written out.
	attachmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	attachmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// Perform image transition when beginning render pass.
	attachmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Setup the depth attachment.
	attachmentDescriptions[1] = attachmentDescriptions[0];
	attachmentDescriptions[1].format = depthFormat;
	attachmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

	attachmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	// Setup the input attachments.
	attachmentDescriptions[2] = attachmentDescriptions[0];
	attachmentDescriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescriptions[2].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	attachmentDescriptions[3] = attachmentDescriptions[2];

	attachmentDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
	// 10-bit normals for more precision.
	attachmentDescriptions[3].format = VK_FORMAT_A2B10G10R10_UNORM_PACK32;

	VkAttachmentReference colorReferences[3] = {
		{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
		{ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL },
	};
	VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	// In the second subpass, we will perform read-only depth testing, as well as reading depth as an input attachment.
	VkAttachmentReference depthReadOnlyReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL };

	VkAttachmentReference inputReferences[3] = {
		{ 2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // Position
		{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL }, // Depth
		{ 3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL }, // Normal
	};

	// We have two subpasses.
	VkSubpassDescription subpassDescriptions[2] = {};
	// This one has 3 color attachments and 1 depth attachment.
	// Render to emissive, albedo and normal as color attachments.
	subpassDescriptions[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescriptions[0].colorAttachmentCount = 3;
	subpassDescriptions[0].pColorAttachments = colorReferences;
	subpassDescriptions[0].pDepthStencilAttachment = &depthReference;
	// This one has 3 input attachments and 1 color attachment.
	// From tile, we read albedo, depth and normal.
	subpassDescriptions[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescriptions[1].inputAttachmentCount = 3;
	subpassDescriptions[1].pInputAttachments = inputReferences;
	subpassDescriptions[1].colorAttachmentCount = 1;
	subpassDescriptions[1].pColorAttachments = colorReferences;
	subpassDescriptions[1].pDepthStencilAttachment = &depthReadOnlyReference;

	// Create a by region dependency between the subpasses.
	VkSubpassDependency subpassDependencies[2] = {};

	// Create an external dependency to wait on the correct pipeline stages before we can transition from
	// UNDEFINED to the appropriate layouts in first subpass.
	subpassDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies[0].dstSubpass = 0;
	subpassDependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
	                                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependencies[0].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[0].srcAccessMask =
	    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	subpassDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
	                                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
	                                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
	subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	subpassDependencies[1].srcSubpass = 0;
	subpassDependencies[1].dstSubpass = 1;
	subpassDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
	                                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	subpassDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies[1].srcAccessMask =
	    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	subpassDependencies[1].dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	subpassDependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Finally, create the renderpass.
	VkRenderPassCreateInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	renderPassInfo.attachmentCount = 4;
	renderPassInfo.pAttachments = attachmentDescriptions;
	renderPassInfo.subpassCount = 2;
	renderPassInfo.pSubpasses = subpassDescriptions;
	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = subpassDependencies;

	VK_CHECK(vkCreateRenderPass(pContext->getDevice(), &renderPassInfo, nullptr, &renderPass));
}

void Multipass::createDebugPipeline()
{
	VkDevice device = pContext->getDevice();

	// Load our SPIR-V shaders.
	VkPipelineShaderStageCreateInfo shaderStageInfos[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
	};

	shaderStageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageInfos[0].module = loadShaderModule(device, "shaders/debug.vert.spv");
	shaderStageInfos[0].pName = "main";
	shaderStageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageInfos[1].module = loadShaderModule(device, "shaders/debug.frag.spv");
	shaderStageInfos[1].pName = "main";

	// We use one vertex buffer, with a stride sizeof(vec2).
	VkVertexInputBindingDescription vertexBindingDescriptions[1] = {};
	vertexBindingDescriptions[0].binding = 0;
	vertexBindingDescriptions[0].stride = 2 * sizeof(float);
	vertexBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Specify the position attribute.
	VkVertexInputAttributeDescription vertexAttributeDescriptions[1] = {};
	// Position in shader specifies layout(location = 0) to link with this attribute.
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].binding = vertexBindingDescriptions[0].binding;
	vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
	vertexAttributeDescriptions[0].offset = 0;

	VkPipelineVertexInputStateCreateInfo vertexStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};
	vertexStateInfo.vertexBindingDescriptionCount = 1;
	vertexStateInfo.pVertexBindingDescriptions = vertexBindingDescriptions;
	vertexStateInfo.vertexAttributeDescriptionCount = 1;
	vertexStateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions;

	// Specify that we will use triangle list to draw geometry.
	VkPipelineInputAssemblyStateCreateInfo assemblyInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
	};
	assemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

	// We will have one viewport and scissor box.
	VkPipelineViewportStateCreateInfo viewportStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.scissorCount = 1;

	// Specify rasterization state.
	VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
	};
	rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateInfo.lineWidth = 1.0f;

	// No multisampling.
	VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
	};
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Our attachment will accumulate lighting to all color channels.
	VkPipelineColorBlendAttachmentState colorBlendState = {};
	colorBlendState.colorWriteMask = 0xf;

	VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
	};
	colorBlendStateInfo.attachmentCount = 1;
	colorBlendStateInfo.pAttachments = &colorBlendState;

	// Disable depth testing and enable stencil testing.
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
	};

	// Specify that these states will be dynamic, i.e. not part of pipeline state object.
	static const VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicStateInfo.dynamicStateCount = 2;
	dynamicStateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStageInfos;
	pipelineInfo.pVertexInputState = &vertexStateInfo;
	pipelineInfo.pInputAssemblyState = &assemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizationStateInfo;
	pipelineInfo.pMultisampleState = &multisampleStateInfo;
	pipelineInfo.pColorBlendState = &colorBlendStateInfo;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayoutLighting;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 1;

	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &debugPipeline));

	// Pipeline is baked, we can delete the shader modules now.
	vkDestroyShaderModule(device, shaderStageInfos[0].module, nullptr);
	vkDestroyShaderModule(device, shaderStageInfos[1].module, nullptr);
}

void Multipass::createLightPipeline()
{
	VkDevice device = pContext->getDevice();

	// Load our SPIR-V shaders.
	VkPipelineShaderStageCreateInfo shaderStageInfos[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
	};

	shaderStageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageInfos[0].module = loadShaderModule(device, "shaders/light.vert.spv");
	shaderStageInfos[0].pName = "main";
	shaderStageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageInfos[1].module = loadShaderModule(device, "shaders/light.frag.spv");
	shaderStageInfos[1].pName = "main";

	// We use one vertex buffer, with a stride sizeof(vec4).
	VkVertexInputBindingDescription vertexBindingDescriptions[1] = {};
	vertexBindingDescriptions[0].binding = 0;
	vertexBindingDescriptions[0].stride = sizeof(CubeVertex);
	vertexBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// Specify the position attribute.
	VkVertexInputAttributeDescription vertexAttributeDescriptions[1] = {};
	// Position in shader specifies layout(location = 0) to link with this attribute.
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].binding = vertexBindingDescriptions[0].binding;
	vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributeDescriptions[0].offset = 0;

	VkPipelineVertexInputStateCreateInfo vertexStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};
	vertexStateInfo.vertexBindingDescriptionCount = 1;
	vertexStateInfo.pVertexBindingDescriptions = vertexBindingDescriptions;
	vertexStateInfo.vertexAttributeDescriptionCount = 1;
	vertexStateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions;

	// Specify that we will use triangle list to draw geometry.
	VkPipelineInputAssemblyStateCreateInfo assemblyInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
	};
	assemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// We will have one viewport and scissor box.
	VkPipelineViewportStateCreateInfo viewportStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.scissorCount = 1;

	// Specify rasterization state.
	VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
	};
	rasterizationStateInfo.depthClampEnable = false;
	rasterizationStateInfo.rasterizerDiscardEnable = false;
	rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateInfo.depthBiasEnable = false;
	rasterizationStateInfo.lineWidth = 1.0f;

	// No multisampling.
	VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
	};
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Our attachment will accumulate lighting to all color channels.
	VkPipelineColorBlendAttachmentState colorBlendState = {};
	colorBlendState.blendEnable = true;
	colorBlendState.colorWriteMask = 0xf;
	colorBlendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendState.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
	};
	colorBlendStateInfo.attachmentCount = 1;
	colorBlendStateInfo.pAttachments = &colorBlendState;

	// Disable depth testing and enable stencil testing.
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
	};
	depthStencilInfo.depthTestEnable = true;
	depthStencilInfo.depthWriteEnable = false;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilInfo.depthBoundsTestEnable = false;
	depthStencilInfo.stencilTestEnable = true;
	depthStencilInfo.front.passOp = VK_STENCIL_OP_KEEP;
	depthStencilInfo.front.failOp = VK_STENCIL_OP_KEEP;
	depthStencilInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
	depthStencilInfo.front.compareOp = VK_COMPARE_OP_EQUAL;
	depthStencilInfo.front.compareMask = 0xff;
	depthStencilInfo.front.writeMask = 0x0;
	depthStencilInfo.front.reference = 1;
	depthStencilInfo.back = depthStencilInfo.front;

	// Specify that these states will be dynamic, i.e. not part of pipeline state object.
	static const VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicStateInfo.dynamicStateCount = 2;
	dynamicStateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStageInfos;
	pipelineInfo.pVertexInputState = &vertexStateInfo;
	pipelineInfo.pInputAssemblyState = &assemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizationStateInfo;
	pipelineInfo.pMultisampleState = &multisampleStateInfo;
	pipelineInfo.pColorBlendState = &colorBlendStateInfo;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayoutLighting;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 1;

	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &lightPipeline));

	// When camera is inside the light cubes we might not rasterize the front face, so draw back faces and invert the depth test.
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
	rasterizationStateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &lightPipelineInside));

	// Pipeline is baked, we can delete the shader modules now.
	vkDestroyShaderModule(device, shaderStageInfos[0].module, nullptr);
	vkDestroyShaderModule(device, shaderStageInfos[1].module, nullptr);
}

void Multipass::createGBufferPipeline()
{
	VkDevice device = pContext->getDevice();

	// Load our SPIR-V shaders.
	VkPipelineShaderStageCreateInfo shaderStageInfos[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
	};

	shaderStageInfos[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageInfos[0].module = loadShaderModule(device, "shaders/geometry.vert.spv");
	shaderStageInfos[0].pName = "main";
	shaderStageInfos[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageInfos[1].module = loadShaderModule(device, "shaders/geometry.frag.spv");
	shaderStageInfos[1].pName = "main";

	// We have three vertex buffers. The position buffer with stride sizeof(float)*4, the
	// tex coord buffers with stride sizeof(float)*2, the normal buffer with stride
	// sizeof(float)*3
	VkVertexInputBindingDescription vertexBindingDescriptions[2] = {};
	vertexBindingDescriptions[0].binding = 0;
	vertexBindingDescriptions[0].stride = sizeof(CubeVertex);
	vertexBindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	vertexBindingDescriptions[1].binding = 1;
	vertexBindingDescriptions[1].stride = sizeof(vec4);
	vertexBindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

	// Specify our three attributes, Position, TexCoord and Normal
	VkVertexInputAttributeDescription vertexAttributeDescriptions[4] = {};
	// Position in shader specifies layout(location = 0) to link with this attribute.
	vertexAttributeDescriptions[0].location = 0;
	vertexAttributeDescriptions[0].binding = vertexBindingDescriptions[0].binding;
	vertexAttributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributeDescriptions[0].offset = 0;
	// TexCoord in shader specifies layout(location = 1) to link with this attribute.
	vertexAttributeDescriptions[1].location = 1;
	vertexAttributeDescriptions[1].binding = 0;
	vertexAttributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	vertexAttributeDescriptions[1].offset = offsetof(CubeVertex, tex);
	// Normal in shader specifies layout(location = 2) to link with this attribute.
	vertexAttributeDescriptions[2].location = 2;
	vertexAttributeDescriptions[2].binding = 0;
	vertexAttributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributeDescriptions[2].offset = offsetof(CubeVertex, normal);
	// InstanceOffset in shader specifies layout(location = 3) to link with this attribute.
	vertexAttributeDescriptions[3].location = 3;
	vertexAttributeDescriptions[3].binding = 1;
	vertexAttributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexAttributeDescriptions[3].offset = 0;

	VkPipelineVertexInputStateCreateInfo vertexStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};
	vertexStateInfo.vertexBindingDescriptionCount = 2;
	vertexStateInfo.pVertexBindingDescriptions = vertexBindingDescriptions;
	vertexStateInfo.vertexAttributeDescriptionCount = 4;
	vertexStateInfo.pVertexAttributeDescriptions = vertexAttributeDescriptions;

	// Specify we will use triangle list to draw geometry.
	VkPipelineInputAssemblyStateCreateInfo assemblyInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
	};
	assemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// We will have one viewport and scissor box.
	VkPipelineViewportStateCreateInfo viewportStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportStateInfo.viewportCount = 1;
	viewportStateInfo.scissorCount = 1;

	// Specify rasterization state.
	VkPipelineRasterizationStateCreateInfo rasterizationStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
	};
	rasterizationStateInfo.depthClampEnable = false;
	rasterizationStateInfo.rasterizerDiscardEnable = false;
	rasterizationStateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateInfo.depthBiasEnable = false;
	rasterizationStateInfo.lineWidth = 1.0f;

	// No multisampling.
	VkPipelineMultisampleStateCreateInfo multisampleStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO
	};
	multisampleStateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Our attachments will write to all color channels, but no blending is enabled.
	VkPipelineColorBlendAttachmentState colorBlendStates[3] = {};
	colorBlendStates[0].blendEnable = false;
	colorBlendStates[0].colorWriteMask = 0xf;
	colorBlendStates[1] = colorBlendStates[0];
	colorBlendStates[2] = colorBlendStates[0];

	VkPipelineColorBlendStateCreateInfo colorBlendStateInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO
	};
	colorBlendStateInfo.attachmentCount = 3;
	colorBlendStateInfo.pAttachments = colorBlendStates;

	// Enable all depth testing and stencil testing.
	VkPipelineDepthStencilStateCreateInfo depthStencilInfo = {
		VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO
	};
	depthStencilInfo.depthTestEnable = true;
	depthStencilInfo.depthWriteEnable = true;
	depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilInfo.depthBoundsTestEnable = false;
	depthStencilInfo.stencilTestEnable = true;
	depthStencilInfo.front.passOp = VK_STENCIL_OP_REPLACE;
	depthStencilInfo.front.failOp = VK_STENCIL_OP_KEEP;
	depthStencilInfo.front.depthFailOp = VK_STENCIL_OP_KEEP;
	depthStencilInfo.front.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilInfo.front.compareMask = 0xff;
	depthStencilInfo.front.writeMask = 0xff;
	depthStencilInfo.front.reference = 1;
	depthStencilInfo.back = depthStencilInfo.front;

	// Specify that these states will be dynamic, i.e. not part of pipeline state object.
	static const VkDynamicState dynamicStates[2] = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
	};

	VkPipelineDynamicStateCreateInfo dynamicStateInfo = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicStateInfo.dynamicStateCount = 2;
	dynamicStateInfo.pDynamicStates = dynamicStates;

	VkGraphicsPipelineCreateInfo pipelineInfo = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStageInfos;
	pipelineInfo.pVertexInputState = &vertexStateInfo;
	pipelineInfo.pInputAssemblyState = &assemblyInfo;
	pipelineInfo.pViewportState = &viewportStateInfo;
	pipelineInfo.pRasterizationState = &rasterizationStateInfo;
	pipelineInfo.pMultisampleState = &multisampleStateInfo;
	pipelineInfo.pColorBlendState = &colorBlendStateInfo;
	pipelineInfo.pDepthStencilState = &depthStencilInfo;
	pipelineInfo.pDynamicState = &dynamicStateInfo;
	pipelineInfo.layout = pipelineLayoutGBuffer;
	pipelineInfo.renderPass = renderPass;

	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineInfo, nullptr, &pipeline));

	// Pipeline is baked, we can delete the shader modules now.
	vkDestroyShaderModule(device, shaderStageInfos[0].module, nullptr);
	vkDestroyShaderModule(device, shaderStageInfos[1].module, nullptr);
}

void Multipass::updateSwapchain(const vector<VkImage> &backbuffers, const Platform::SwapchainDimensions &dimensions)
{
	VkDevice device = pContext->getDevice();
	width = dimensions.width;
	height = dimensions.height;

	// In case we're reinitializing the swapchain, terminate the old one first.
	termBackbuffers();

	// Create images for storing albedo and normal information between subpasses.
	albedoImage = createImage(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
	                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	                          VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, width, height);

	normalImage = createImage(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
	                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
	                          VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_IMAGE_ASPECT_COLOR_BIT, width, height);

	// Create depth buffer.
	depthImage = createImage(VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
	                             VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
	                         depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, width, height);

	// To read depth as an image attachment we need to restrict the aspect to be just the depth.
	depthImageDepthOnlyView = createImageView(depthImage.image, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	uboAlignment = std::max(size_t(pContext->getPlatform().getGpuProperties().limits.minUniformBufferOffsetAlignment),
	                        sizeof(mat4));
	uniformBuffer = createBuffer(nullptr, backbuffers.size() * uboAlignment, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	VK_CHECK(vkMapMemory(device, uniformBuffer.memory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void **>(&uboData)));

	// We can't initialize descriptors until the images are created.
	createDescriptors();

	// We can't initialize the renderpass until we know the swapchain format.
	createRenderPass(dimensions.format);
	// We can't initialize the pipelines until we know the render pass.
	createGBufferPipeline();
	createLightPipeline();
	createDebugPipeline();

	// For all backbuffers in the swapchain ...
	for (auto image : backbuffers)
	{
		Backbuffer backbuffer;
		backbuffer.image = image;

		// Create an image view which we can render into.
		VkImageViewCreateInfo imageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = dimensions.format;
		imageViewInfo.image = image;
		imageViewInfo.subresourceRange.baseMipLevel = 0;
		imageViewInfo.subresourceRange.baseArrayLayer = 0;
		imageViewInfo.subresourceRange.levelCount = 1;
		imageViewInfo.subresourceRange.layerCount = 1;
		imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewInfo.components.a = VK_COMPONENT_SWIZZLE_A;

		VK_CHECK(vkCreateImageView(device, &imageViewInfo, nullptr, &backbuffer.view));

		// Build the framebuffer.
		VkImageView attachments[4] = { backbuffer.view, depthImage.view, albedoImage.view, normalImage.view };

		VkFramebufferCreateInfo framebufferInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = 4;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = width;
		framebufferInfo.height = height;
		framebufferInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, nullptr, &backbuffer.framebuffer));

		this->backbuffers.push_back(backbuffer);
	}
}

void Multipass::render(unsigned swapchainIndex, float deltaTime)
{
	// Render to this backbuffer.
	Backbuffer backbuffer = backbuffers[swapchainIndex];

	// Request a fresh command buffer.
	VkCommandBuffer commandBuffer = pContext->requestPrimaryCommandBuffer();

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo bufferBeginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &bufferBeginInfo);

	// Set clear colors and clear depth values. Since we have 4 attachments we need 4 VkClearValue.
	// Depth/Stencil is attachment 1.
	VkClearValue clears[4] = {};
	clears[0].color.float32[0] = 0.1f;
	clears[0].color.float32[1] = 0.1f;
	clears[0].color.float32[2] = 0.2f;
	clears[0].color.float32[3] = 1.0f;
	clears[1].depthStencil.depth = 1.0f;
	clears[1].depthStencil.stencil = 0;

	// Begin the render pass.
	VkRenderPassBeginInfo renderPassBeginInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = backbuffer.framebuffer;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 4;
	renderPassBeginInfo.pClearValues = clears;
	vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the descriptor sets.
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutGBuffer, 0, 1,
	                        &descriptorSets[0], 0, nullptr);

	// Bind the graphics pipeline.
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	// Set up dynamic state.
	// Viewport.
	VkViewport viewport = { 0 };
	viewport.width = float(width);
	viewport.height = float(height);
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	// Scissor box.
	VkRect2D scissor;
	memset(&scissor, 0, sizeof(scissor));
	scissor.extent.width = width;
	scissor.extent.height = height;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	// Bind the vertex buffers.
	VkDeviceSize offsets[2] = {};
	VkBuffer buffers[] = { vertexBuffer.buffer, perInstanceBuffer.buffer };
	vkCmdBindVertexBuffers(commandBuffer, 0, 2, buffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

	// Update the push constans.
	float aspect = float(width) / height;

	// Simple perspective projection.
	mat4 projection = perspective(radians(60.0f), aspect, 1.0f, 500.0f);

	// Set the view matrix.
	mat4 view = lookAt(vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));

	// Create a simple rotation matrix which rotates around all axes.
	totalTime += deltaTime;
	mat4 model = rotate(0.25f * totalTime, vec3(1.0f, 1.0f, 1.0f));

	// Fix the projection matrix so it matches what Vulkan expects.
	mat4 mvp[2] = { model, vulkanStyleProjection(projection) * view };

	vkCmdPushConstants(commandBuffer, pipelineLayoutGBuffer, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mvp), &mvp);

	// Draw the cube with lots of instances in a grid shape.
	vkCmdDrawIndexed(commandBuffer, 36, NUM_INSTANCES_X * NUM_INSTANCES_Y * NUM_INSTANCES_Z, 0, 0, 0);

	// Go to the next subpass, here we do the lighting.
	vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the input attachments, with a dynamic offset into the UBO.
	uint32_t uboOffset = swapchainIndex * uboAlignment;
	memcpy(uboData + uboOffset, &mvp[1], sizeof(mat4));
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayoutLighting, 0, 2,
	                        &descriptorSets[1], 1, &uboOffset);

	// Use the cube mesh as a light volume as well.
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

	// These cubes do not intersect with the camera.
	static const vec4 lightPositions[] = {
		vec4(-20.0f, 20.0f, -30.0f, 25.0f), vec4(-20.0f, -20.0f, -30.0f, 20.0f), vec4(+20.0f, +20.0f, -15.0f, 12.0f),
		vec4(+20.0f, -20.0f, -15.0f, 12.0f),
	};

	static const vec4 lightColors[] = {
		vec4(5.0f, 2.5f, 0.0f, 1.0f), vec4(0.0f, 2.5f, 5.0f, 1.0f), vec4(0.0f, 3.5f, 0.0f, 1.0f),
		vec4(0.5f, 0.5f, 0.5f, 1.0f),
	};

	static const vec4 lightColorsInside[] = {
		vec4(1.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 0.0f, 1.0f), vec4(0.0f, 0.0f, 1.0f, 1.0f),
		vec4(0.0f, 0.5f, 0.5f, 1.0f),
	};

	// These cubes intersect with the camera.
	static vec4 lightPositionsInside[] = {
		vec4(-10.0f, +10.0f, -10.0f, 20.0f), vec4(-10.0f, -10.0f, -10.0f, 20.0f), vec4(+10.0f, +10.0f, -10.0f, 20.0f),
		vec4(+5.0f, +5.0f, 10.0f, 30.0f),
	};

	LightingData light;
	light.invViewProj = inverse(mvp[1]);
	light.invResolution = vec2(1.0f / width, 1.0f / height);

	if (fract(totalTime / 15.0f) < 0.5f)
	{
		// Bind the other pipeline for use in the second subpass.
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPipeline);

		// Set up dynamic state.
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		for (unsigned i = 0; i < 4; i++)
		{
			// Draw each light. Not instancing here means we get to use push constants for the per light data, which is very nice as long as
			// the number of draw calls doesn't outweigh the advantage of push constants for everything in fragment.
			light.color = lightColors[i];
			light.position = lightPositions[i];
			vkCmdPushConstants(commandBuffer, pipelineLayoutLighting,
			                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(light), &light);

			// Draw the cube with one instance.
			vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, 0);
		}

		// Bind the other light pipeline for use in the second subpass.
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightPipelineInside);

		// Set up dynamic state.
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		for (unsigned i = 0; i < 4; i++)
		{
			// Draw each light. Not instancing here means we get to use push constants for the per light data, which is very nice as long as
			// the number of draw calls doesn't outweigh the advantage of push constants for everything in fragment.
			light.color = lightColorsInside[i];
			light.position = lightPositionsInside[i];
			vkCmdPushConstants(commandBuffer, pipelineLayoutLighting,
			                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(light), &light);

			// Draw the cube with one instance.
			vkCmdDrawIndexed(commandBuffer, 36, 1, 0, 0, 0);
		}
	}
	else
	{
		// Occasionally, show a debug view of albedo/depth/normals.
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debugPipeline);

		vkCmdPushConstants(commandBuffer, pipelineLayoutLighting,
		                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(light), &light);

		// Set up dynamic state.
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &quadVertexBuffer.buffer, offsets);
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	}

	// Complete the render pass.
	vkCmdEndRenderPass(commandBuffer);

	// Complete the command buffer.
	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	// Submit it to the queue.
	pContext->submitSwapchain(commandBuffer);
}

void Multipass::termBackbuffers()
{
	// Tear down backbuffers.
	// If our swapchain changes, we will call this and create a new swapchain.
	VkDevice device = pContext->getDevice();

	if (!backbuffers.empty())
	{
		// Wait until device is idle before teardown.
		vkQueueWaitIdle(pContext->getGraphicsQueue());
		for (auto &buffer : backbuffers)
		{
			vkDestroyFramebuffer(device, buffer.framebuffer, nullptr);
			vkDestroyImageView(device, buffer.view, nullptr);
		}
		backbuffers.clear();
		if (renderPass != VK_NULL_HANDLE)
			vkDestroyRenderPass(device, renderPass, nullptr);
		if (pipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(device, pipeline, nullptr);
		if (lightPipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(device, lightPipeline, nullptr);
		if (lightPipelineInside != VK_NULL_HANDLE)
			vkDestroyPipeline(device, lightPipelineInside, nullptr);
		if (debugPipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(device, debugPipeline, nullptr);

		// Depth, albedo and normal images.
		vkDestroyImageView(device, depthImage.view, nullptr);
		vkDestroyImageView(device, depthImageDepthOnlyView, nullptr);
		vkDestroyImage(device, depthImage.image, nullptr);
		vkFreeMemory(device, depthImage.memory, nullptr);
		vkDestroyImageView(device, albedoImage.view, nullptr);
		vkDestroyImage(device, albedoImage.image, nullptr);
		vkFreeMemory(device, albedoImage.memory, nullptr);
		vkDestroyImageView(device, normalImage.view, nullptr);
		vkDestroyImage(device, normalImage.image, nullptr);
		vkFreeMemory(device, normalImage.memory, nullptr);
	}

	if (uniformBuffer.buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(device, uniformBuffer.buffer, nullptr);
	if (uniformBuffer.memory != VK_NULL_HANDLE)
	{
		vkUnmapMemory(device, uniformBuffer.memory);
		vkFreeMemory(device, uniformBuffer.memory, nullptr);
	}
	uniformBuffer = {};
}

void Multipass::terminate()
{
	// Teardown.
	VkDevice device = pContext->getDevice();
	if (device != VK_NULL_HANDLE)
		vkDeviceWaitIdle(device);

	termBackbuffers();

	// Vertex buffer.
	if (vertexBuffer.buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(device, vertexBuffer.buffer, nullptr);

	if (vertexBuffer.memory != VK_NULL_HANDLE)
		vkFreeMemory(device, vertexBuffer.memory, nullptr);

	// Index buffer.
	if (indexBuffer.buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(device, indexBuffer.buffer, nullptr);

	if (indexBuffer.memory != VK_NULL_HANDLE)
		vkFreeMemory(device, indexBuffer.memory, nullptr);

	// Instance buffer.
	if (perInstanceBuffer.buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(device, perInstanceBuffer.buffer, nullptr);

	if (perInstanceBuffer.memory != VK_NULL_HANDLE)
		vkFreeMemory(device, perInstanceBuffer.memory, nullptr);

	// Vertex buffer for the quad.
	if (quadVertexBuffer.buffer != VK_NULL_HANDLE)
		vkDestroyBuffer(device, quadVertexBuffer.buffer, nullptr);

	if (quadVertexBuffer.memory != VK_NULL_HANDLE)
		vkFreeMemory(device, quadVertexBuffer.memory, nullptr);

	// Texture.
	vkDestroyImageView(device, texture.view, nullptr);
	vkDestroyImage(device, texture.image, nullptr);
	vkDestroySampler(device, texture.sampler, nullptr);
	vkFreeMemory(device, texture.memory, nullptr);

	// Resources.
	if (descriptorPool != VK_NULL_HANDLE)
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);

	if (pipelineLayoutGBuffer != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(device, pipelineLayoutGBuffer, nullptr);
	if (pipelineLayoutLighting != VK_NULL_HANDLE)
		vkDestroyPipelineLayout(device, pipelineLayoutLighting, nullptr);

	for (auto &layout : setLayouts)
		if (layout != VK_NULL_HANDLE)
			vkDestroyDescriptorSetLayout(device, layout, nullptr);

	if (pipelineCache != VK_NULL_HANDLE)
		vkDestroyPipelineCache(device, pipelineCache, nullptr);
}

VulkanApplication *MaliSDK::createApplication()
{
	return new Multipass();
}
