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

#include "framework/application.hpp"
#include "framework/assets.hpp"
#include "framework/common.hpp"
#include "framework/context.hpp"
#include "framework/math.hpp"
#include "platform/platform.hpp"
#include <string.h>

using namespace MaliSDK;
using namespace std;
using glm::vec2;
using glm::mat4;
using glm::ortho;

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

struct MipLevel
{
	// The raw, uncompressed data for the image.
	vector<uint8_t> buffer;

	// The buffer to be copied into a level of the texture.
	Buffer stagingBuffer;

	unsigned width, height;
};

struct UniformBufferData
{
	// The MVP matrix.
	mat4 mvp;

	// The index of the quad to be highlighted.
	int32_t highlightedQuad;

	// The current type of mipmaps.
	int32_t mipmapType;
};

// We have one PerFrame struct for every swapchain image.
// Every swapchain image will have its own uniform buffer and descriptor set.
struct PerFrame
{
	Buffer uniformBuffer;
	VkDescriptorSet descriptorSet;
	VkDescriptorPool descriptorPool;
};

struct Vertex
{
	vec2 position;
	vec2 texCoord;
};

class Mipmapping : public VulkanApplication
{
public:
	virtual bool initialize(Context *pContext);
	virtual void render(unsigned swapchainIndex, float deltaTime);
	virtual void terminate();
	virtual void updateSwapchain(const vector<VkImage> &backbuffers, const Platform::SwapchainDimensions &dim);

private:
	Context *pContext = nullptr;

	vector<Backbuffer> backbuffers;
	vector<PerFrame> perFrame;
	unsigned width, height;

	// The renderpass description.
	VkRenderPass renderPass;

	// The graphics pipeline.
	VkPipeline pipeline;

	// Pipeline objects can be cached in a pipeline cache.
	// Mostly useful when you have many pipeline objects.
	VkPipelineCache pipelineCache;

	// Specifies the pipeline layout for resources.
	// We don't use any in this sample, but we still need to provide a dummy one.
	VkPipelineLayout pipelineLayout;

	VkDescriptorSetLayout setLayout;

	Buffer vertexBuffer;
	Buffer indexBuffer;

	// The two mipmapped textures to be shown alternately.
	Texture textures[2];

	// The texture containing the labels for the size and type of mipmaps.
	Texture labelTexture;

	Buffer createBuffer(const void *pInitial, size_t size, VkFlags usage);
	Texture createMipmappedTextureFromAssets(const vector<const char *> pPaths, bool generateMipLevels = false);
	void createQuad(vector<Vertex> &vertexData, vector<uint16_t> &indexData, unsigned &baseIndex, vec2 topLeft, vec2 bottomRight);

	uint32_t findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements);
	uint32_t findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements, uint32_t hostRequirements);

	void initRenderPass(VkFormat format);
	void termBackbuffers();

	void initPerFrame(unsigned numBackbuffers);
	void termPerFrame();

	void initVertexBuffer();
	void initPipeline();
	void initPipelineLayout();

	void imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
	                        VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
	                        VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout,
	                        VkImageLayout newLayout, unsigned baseMipLevel, unsigned mipLevelCount);

	float accumulatedTime = 0.0f;
};

// To create a buffer, both the device and application have requirements from the buffer object.
// Vulkan exposes the different types of buffers the device can allocate, and we have to find a suitable one.
// deviceRequirements is a bitmask expressing which memory types can be used for a buffer object.
// The different memory types' properties must match with what the application wants.
uint32_t Mipmapping::findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements)
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

uint32_t Mipmapping::findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements,
                                                                uint32_t hostRequirements)
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

	// If we cannot find the particular memory type we're looking for, just pick the first one available.
	if (hostRequirements != 0)
		return findMemoryTypeFromRequirements(deviceRequirements, 0);
	else
	{
		LOGE("Failed to obtain suitable memory type.\n");
		abort();
	}
}

Texture Mipmapping::createMipmappedTextureFromAssets(const vector<const char *> pPaths, bool generateMipLevels)
{
	// We first create a vector of staging buffers, containing the images loaded from each path.
	//
	// We will then create a mipmapped texture, depending on the value of generateMipLevels:
	// - if generateMipLevels is true, we will generate mip levels based on the first path specified,
	//   using vkCmdBlitImage;
	// - if generateMipLevels is false, we will copy each buffer into a mip level of an optimally tiled
	//   texture with vkCmdCopyBufferToImage.
	//
	// The layout of the texture is not specified as it is highly GPU-dependent and optimized for
	// utilizing texture caches better.
	vector<MipLevel> mipLevels;
	unsigned mipLevelCount;

	for (auto &pPath : pPaths)
	{
		MipLevel mipLevel;

		if (FAILED(loadRgba8888TextureFromAsset(pPath, &mipLevel.buffer, &mipLevel.width, &mipLevel.height)))
		{
			LOGE("Failed to load texture from asset.\n");
			abort();
		}

		// Copy commands such as vkCmdCopyBufferToImage will need TRANSFER_SRC_BIT.
		mipLevel.stagingBuffer = createBuffer(mipLevel.buffer.data(), mipLevel.width * mipLevel.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		mipLevels.push_back(mipLevel);
	}

	if (generateMipLevels)
	{
		// Get the number of mip levels to be generated, based on the size of the source.
		mipLevelCount = floor(log2(float(min(mipLevels[0].width, mipLevels[0].height)))) + 1;
	}
	else
	{
		// Get the number of mip levels based on the number of loaded sources.
		mipLevelCount = mipLevels.size();
	}

	VkDevice device = pContext->getDevice();
	VkImage image;
	VkDeviceMemory memory;

	// We will transition the actual texture into a proper layout before transfering any data, so leave it as undefined.
	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = VK_FORMAT_R8G8B8A8_UNORM;
	info.extent.width = mipLevels[0].width;
	info.extent.height = mipLevels[0].height;
	info.extent.depth = 1;
	info.mipLevels = mipLevelCount;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = (generateMipLevels ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : 0) | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// Create texture.
	VK_CHECK(vkCreateImage(device, &info, nullptr, &image));

	// Allocate memory for the texture.
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, image, &memReqs);

	VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc.allocationSize = memReqs.size;
	// If a device local memory type exists, we should use that.
	// DEVICE_LOCAL implies that the device has the fastest possible access to this resource, which
	// is clearly what we want here.
	// On integrated GPUs such as Mali, memory types are generally *both* DEVICE_LOCAL and HOST_VISIBLE at the same time,
	// since the GPU can directly access the same memory as the CPU can.
	alloc.memoryTypeIndex =
		findMemoryTypeFromRequirementsWithFallback(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &memory));

	// Bind the newly allocated memory to the image.
	vkBindImageMemory(device, image, memory, 0);

	// Create an image view for the new texture.
	// Note that CreateImageView must happen after BindImageMemory.
	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = image;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	viewInfo.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageView view;
	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view));

	// Now we need to transfer the staging textures into the real texture.
	// For this we will need a command buffer.
	VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Transition the uninitialized texture into a TRANSFER_DST_OPTIMAL layout.
	// We do not need to wait for anything to make the transition, so use TOP_OF_PIPE_BIT as the srcStageMask.
	imageMemoryBarrier(cmd, image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	                   0, mipLevelCount);

	VkBufferImageCopy region = {};
	memset(&region, 0, sizeof(region));
	region.bufferOffset = 0;
	region.bufferRowLength = mipLevels[0].width;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent.width = mipLevels[0].width;
	region.imageExtent.height = mipLevels[0].height;
	region.imageExtent.depth = 1;

	// Copy the buffer for the first mip level to our optimally tiled image.
	vkCmdCopyBufferToImage(cmd, mipLevels[0].stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	if (generateMipLevels)
	{
		// Transition first mip level into a TRANSFER_SRC_OPTIMAL layout.
		// We need to wait for first CopyBuffer to complete before we can transition away from TRANSFER_DST_OPTIMAL,
		// so use VK_PIPELINE_STAGE_TRANSFER_BIT as the srcStageMask.
		imageMemoryBarrier(cmd, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
		                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		                   0, 1);

		for (unsigned i = 1; i < mipLevelCount; i++)
		{
			VkImageBlit region = {};
			memset(&region, 0, sizeof(region));
			region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.srcSubresource.mipLevel = i - 1;
			region.srcSubresource.layerCount = 1;
			region.srcOffsets[1].x = max(mipLevels[0].width >> (i - 1), 1u);
			region.srcOffsets[1].y = max(mipLevels[0].height >> (i - 1), 1u);
			region.srcOffsets[1].z = 1;
			region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.dstSubresource.mipLevel = i;
			region.dstSubresource.layerCount = 1;
			region.dstOffsets[1].x = max(mipLevels[0].width >> i, 1u);
			region.dstOffsets[1].y = max(mipLevels[0].height >> i, 1u);
			region.dstOffsets[1].z = 1;

			// Generate a mip level by copying and scaling the previous one.
			vkCmdBlitImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

			// Transition the previous mip level into a SHADER_READ_ONLY_OPTIMAL layout.
			imageMemoryBarrier(cmd, image, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
			                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			                   i - 1, 1);

			if (i < mipLevelCount)
			{
				// Transition the current mip level into a TRANSFER_SRC_OPTIMAL layout, to be used as the source for the next one.
				imageMemoryBarrier(cmd, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				                   i, 1);
			}
			else
			{
				// If this is the last iteration of the loop, transition the mip level directly to a SHADER_READ_ONLY_OPTIMAL layout.
				imageMemoryBarrier(cmd, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
				                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				                   i, 1);
			}
		}
	}
	else
	{
		for (unsigned i = 1; i < mipLevelCount; i++)
		{
			VkBufferImageCopy region = {};
			memset(&region, 0, sizeof(region));
			region.bufferOffset = 0;
			region.bufferRowLength = mipLevels[i].width;
			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = i;
			region.imageSubresource.layerCount = 1;
			region.imageExtent.width = mipLevels[i].width;
			region.imageExtent.height = mipLevels[i].height;
			region.imageExtent.depth = 1;

			// Copy each staging buffer to the appropriate mip level of our optimally tiled image.
			vkCmdCopyBufferToImage(cmd, mipLevels[i].stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
		}

		// Wait for all transfers to complete before we let any fragment shading begin.
		imageMemoryBarrier(cmd, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		                   0, mipLevelCount);
	}

	VK_CHECK(vkEndCommandBuffer(cmd));
	pContext->submit(cmd);

	// We want to free the staging buffer and memory right away, so wait for GPU complete the transfer.
	vkQueueWaitIdle(pContext->getGraphicsQueue());

	for (auto &mipLevel : mipLevels)
	{
		// Now it's safe to free the temporary resources.
		vkFreeMemory(device, mipLevel.stagingBuffer.memory, nullptr);
		vkDestroyBuffer(device, mipLevel.stagingBuffer.buffer, nullptr);
	}

	// Finally, create a sampler.
	VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
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
		image, view, memory, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler, mipLevels[0].width, mipLevels[0].height,
	};
	return ret;
}

void Mipmapping::imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
                                    VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
                                    VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout,
                                    VkImageLayout newLayout, unsigned baseMipLevel, unsigned mipLevelCount)
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
	barrier.subresourceRange.baseMipLevel = baseMipLevel;
	barrier.subresourceRange.levelCount = mipLevelCount;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, false, 0, nullptr, 0, nullptr, 1, &barrier);
}

Buffer Mipmapping::createBuffer(const void *pInitialData, size_t size, VkFlags usage)
{
	Buffer buffer;
	VkDevice device = pContext->getDevice();

	VkBufferCreateInfo info = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	info.usage = usage;
	info.size = size;

	VK_CHECK(vkCreateBuffer(device, &info, nullptr, &buffer.buffer));

	// Ask device about its memory requirements.
	VkMemoryRequirements memReqs;
	vkGetBufferMemoryRequirements(device, buffer.buffer, &memReqs);

	VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc.allocationSize = memReqs.size;

	// We want host visible and coherent memory to simplify things.
	alloc.memoryTypeIndex = findMemoryTypeFromRequirements(
		memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &buffer.memory));

	// Buffers are not backed by memory, so bind our memory explicitly to the buffer.
	vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);

	// Map the memory and dump data in there.
	if (pInitialData)
	{
		void *pData;
		VK_CHECK(vkMapMemory(device, buffer.memory, 0, size, 0, &pData));
		memcpy(pData, pInitialData, size);
		vkUnmapMemory(device, buffer.memory);
	}

	return buffer;
}

void Mipmapping::initRenderPass(VkFormat format)
{
	VkAttachmentDescription attachment = { 0 };
	// Backbuffer format.
	attachment.format = format;
	// Not multisampled.
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// When starting the frame, we want tiles to be cleared.
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// When ending the frame, we want tiles to be written out.
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// Don't care about stencil since we're not using it.
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// We have one subpass.
	// This subpass has 1 color attachment.
	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkSubpassDescription subpass = { 0 };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;

	// Create a dependency to external events.
	// We need to wait for the WSI semaphore to signal.
	// Only pipeline stages which depend on COLOR_ATTACHMENT_OUTPUT_BIT will
	// actually wait for the semaphore, so we must also wait for that pipeline stage.
	VkSubpassDependency dependency = { 0 };
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// We are creating a write-after-read dependency (presentation must be done reading), so we don't need a memory barrier.
	dependency.srcAccessMask = 0;
	// The layout transition to COLOR_ATTACHMENT_OPTIMAL will imply a memory barrier for the relevant access bits, so we don't have to do it.
	dependency.dstAccessMask = 0;

	// Finally, create the renderpass.
	VkRenderPassCreateInfo rpInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpInfo.attachmentCount = 1;
	rpInfo.pAttachments = &attachment;
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;
	rpInfo.dependencyCount = 1;
	rpInfo.pDependencies = &dependency;
	VK_CHECK(vkCreateRenderPass(pContext->getDevice(), &rpInfo, nullptr, &renderPass));
}

void Mipmapping::createQuad(vector<Vertex> &vertexData, vector<uint16_t> &indexData, unsigned &baseIndex, vec2 topLeft, vec2 bottomRight)
{
	vec2 bottomLeft = vec2(topLeft.x, bottomRight.y);
	vec2 topRight = vec2(bottomRight.x, topLeft.y);

	// Add the vertices to vertexData.
	vertexData.push_back( { topLeft,     vec2(0.0f, 0.0f) } );
	vertexData.push_back( { bottomLeft,  vec2(0.0f, 1.0f) } );
	vertexData.push_back( { topRight,    vec2(1.0f, 0.0f) } );
	vertexData.push_back( { bottomRight, vec2(1.0f, 1.0f) } );

	// Add the indexes to indexData.
	indexData.push_back(baseIndex);
	indexData.push_back(baseIndex + 1);
	indexData.push_back(baseIndex + 2);
	indexData.push_back(baseIndex + 3);
	indexData.push_back(baseIndex + 2);
	indexData.push_back(baseIndex + 1);

	baseIndex += 4;
}

void Mipmapping::initVertexBuffer()
{
	static vector<Vertex> vertexData;
	static vector<uint16_t> indexData;
	unsigned baseIndex = 0;

	// Create a set of quads of decreasing size.
	createQuad(vertexData, indexData, baseIndex, vec2(-1.35f, +0.8f), vec2(-0.35f, -0.2f));

	for (unsigned i = 1; i < 10; i++)
	{
		float quadSize = 1.0f / (1 << i); // 2 ^ (-i)
		createQuad(vertexData, indexData, baseIndex, vec2(-0.35f - 2.0f * quadSize, -0.2f), vec2(-0.35f - quadSize, -0.2f - quadSize));
	}

	// Create a single large quad to show mip level stretching.
	createQuad(vertexData, indexData, baseIndex, vec2(+0.0f, +0.8f), vec2(+1.5f, -0.7f));

	// Create a quad for a label showing the size of the current mip level.
	createQuad(vertexData, indexData, baseIndex, vec2(+0.0f, -0.75f), vec2(+1.5f, -0.9f));

	// Create a quad for a label showing the current type of mipmaps.
	createQuad(vertexData, indexData, baseIndex, vec2(-1.6f, -0.75f), vec2(-0.1f, -0.9f));

	vertexBuffer = createBuffer(vertexData.data(), sizeof(vertexData[0]) * vertexData.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
	indexBuffer = createBuffer(indexData.data(), sizeof(indexData[0]) * indexData.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void Mipmapping::initPipelineLayout()
{
	VkDevice device = pContext->getDevice();

	// In our fragment shader, we have two textures with layout(set = 0, binding = {0, 1}).
	VkDescriptorSetLayoutBinding bindings[3] = { { 0 } };
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	// In our vertex shader, we have one uniform buffer with layout(set = 0, binding = 2).
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[2].descriptorCount = 1;
	bindings[2].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	// Create the descriptor set layout.
	VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	info.bindingCount = 3;
	info.pBindings = bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout));

	// We have one descriptor set in our layout.
	VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &setLayout;
	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));
}

void Mipmapping::initPipeline()
{
	VkDevice device = pContext->getDevice();

	// Specify we will use triangle strip to draw geometry.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
	};
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// Specify our two attributes, Position and TexCoord.
	VkVertexInputAttributeDescription attributes[2] = { { 0 } };
	attributes[0].location = 0; // Position in shader specifies layout(location = 0) to link with this attribute.
	attributes[0].binding = 0;
	attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[0].offset = 0;
	attributes[1].location = 1; // TexCoord in shader specifies layout(location = 1) to link with this attribute.
	attributes[1].binding = 0;
	attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[1].offset = 2 * sizeof(float);

	// We have one vertex buffer, with stride 8 floats (vec4 + vec4).
	VkVertexInputBindingDescription binding = { 0 };
	binding.binding = 0;
	binding.stride = sizeof(Vertex);
	binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vertexInput = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};
	vertexInput.vertexBindingDescriptionCount = 1;
	vertexInput.pVertexBindingDescriptions = &binding;
	vertexInput.vertexAttributeDescriptionCount = 2;
	vertexInput.pVertexAttributeDescriptions = attributes;

	// Specify rasterization state.
	VkPipelineRasterizationStateCreateInfo raster = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO
	};
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_BACK_BIT;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.depthClampEnable = false;
	raster.rasterizerDiscardEnable = false;
	raster.depthBiasEnable = false;
	raster.lineWidth = 1.0f;

	// Our attachment will write to all color channels, with blending based on the alpha channel.
	VkPipelineColorBlendAttachmentState blendAttachment = { 0 };
	blendAttachment.blendEnable = true;
	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	blendAttachment.blendEnable = true;
	blendAttachment.colorWriteMask = 0xf;

	VkPipelineColorBlendStateCreateInfo blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend.attachmentCount = 1;
	blend.pAttachments = &blendAttachment;

	// We will have one viewport and scissor box.
	VkPipelineViewportStateCreateInfo viewport = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;

	// Disable all depth testing.
	VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencil.depthTestEnable = false;
	depthStencil.depthWriteEnable = false;
	depthStencil.depthBoundsTestEnable = false;
	depthStencil.stencilTestEnable = false;

	// No multisampling.
	VkPipelineMultisampleStateCreateInfo multisample = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Specify that these states will be dynamic, i.e. not part of pipeline state object.
	static const VkDynamicState dynamics[] = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
	};
	VkPipelineDynamicStateCreateInfo dynamic = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamic.pDynamicStates = dynamics;
	dynamic.dynamicStateCount = sizeof(dynamics) / sizeof(dynamics[0]);

	// Load our SPIR-V shaders.
	VkPipelineShaderStageCreateInfo shaderStages[2] = {
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
	};

	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].module = loadShaderModule(device, "shaders/textured.vert.spv");
	shaderStages[0].pName = "main";
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = loadShaderModule(device, "shaders/textured.frag.spv");
	shaderStages[1].pName = "main";

	VkGraphicsPipelineCreateInfo pipe = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
	pipe.stageCount = 2;
	pipe.pStages = shaderStages;
	pipe.pVertexInputState = &vertexInput;
	pipe.pInputAssemblyState = &inputAssembly;
	pipe.pRasterizationState = &raster;
	pipe.pColorBlendState = &blend;
	pipe.pMultisampleState = &multisample;
	pipe.pViewportState = &viewport;
	pipe.pDepthStencilState = &depthStencil;
	pipe.pDynamicState = &dynamic;
	pipe.renderPass = renderPass;
	pipe.layout = pipelineLayout;

	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipe, nullptr, &pipeline));

	// Pipeline is baked, we can delete the shader modules now.
	vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
	vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
}

bool Mipmapping::initialize(Context *pContext)
{
	this->pContext = pContext;

	// Create the vertex buffer.
	initVertexBuffer();

	// Initialize the pipeline layout.
	initPipelineLayout();

	// Load texture with pre-generated mipmaps.
	vector<char const *> pPaths = { "textures/T_Speaker_512.png", "textures/T_Speaker_256.png", "textures/T_Speaker_128.png",
	                                "textures/T_Speaker_64.png",  "textures/T_Speaker_32.png",  "textures/T_Speaker_16.png",
	                                "textures/T_Speaker_8.png",   "textures/T_Speaker_4.png",   "textures/T_Speaker_2.png",
	                                "textures/T_Speaker_1.png" };
	textures[0] = createMipmappedTextureFromAssets(pPaths, false);

	// Load texture and generate mipmaps.
	textures[1] = createMipmappedTextureFromAssets({ "textures/T_Pedestal_512.png" }, true);

	// Load the texture for the labels.
	labelTexture = createMipmappedTextureFromAssets({ "textures/labels.png" }, false);

	// Create a pipeline cache (although we'll only create one pipeline).
	VkPipelineCacheCreateInfo pipelineCacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	VK_CHECK(vkCreatePipelineCache(pContext->getDevice(), &pipelineCacheInfo, nullptr, &pipelineCache));

	return true;
}

void Mipmapping::render(unsigned swapchainIndex, float deltaTime)
{
	// Render to this backbuffer.
	Backbuffer &backbuffer = backbuffers[swapchainIndex];
	PerFrame &frame = perFrame[swapchainIndex];

	// Request a fresh command buffer.
	VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Set clear color values.
	VkClearValue clearValue;
	clearValue.color.float32[0] = 0.1f;
	clearValue.color.float32[1] = 0.1f;
	clearValue.color.float32[2] = 0.2f;
	clearValue.color.float32[3] = 1.0f;

	// Begin the render pass.
	VkRenderPassBeginInfo rpBegin = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rpBegin.renderPass = renderPass;
	rpBegin.framebuffer = backbuffer.framebuffer;
	rpBegin.renderArea.extent.width = width;
	rpBegin.renderArea.extent.height = height;
	rpBegin.clearValueCount = 1;
	rpBegin.pClearValues = &clearValue;
	vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the graphics pipeline.
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	// Set up dynamic state.
	// Viewport
	VkViewport vp = { 0 };
	vp.x = 0.0f;
	vp.y = 0.0f;
	vp.width = float(width);
	vp.height = float(height);
	vp.minDepth = 0.0f;
	vp.maxDepth = 1.0f;
	vkCmdSetViewport(cmd, 0, 1, &vp);

	// Scissor box
	VkRect2D scissor;
	memset(&scissor, 0, sizeof(scissor));
	scissor.extent.width = width;
	scissor.extent.height = height;
	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// Bind vertex buffer.
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer.buffer, &offset);

	// Bind index buffer.
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

	// Select one of the two textures based on the elapsed time.
	accumulatedTime += deltaTime;
	int textureIndex = static_cast<int>(accumulatedTime) / 10 % 2;

	// Update the texture descriptor.
	VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
	VkDescriptorImageInfo imageInfo = { textures[textureIndex].sampler, textures[textureIndex].view, textures[textureIndex].layout };

	write.dstSet = frame.descriptorSet;
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(pContext->getDevice(), 1, &write, 0, nullptr);

	// Bind the descriptor set.
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &frame.descriptorSet, 0,
	                        nullptr);

	// Update the uniform buffers memory.
	UniformBufferData *bufData = nullptr;
	VK_CHECK(vkMapMemory(pContext->getDevice(), frame.uniformBuffer.memory, 0, VK_WHOLE_SIZE, 0,
	         reinterpret_cast<void **>(&bufData)));

	// Simple orthographic projection.
	float aspect = float(width) / height;
	mat4 proj = ortho(aspect * -1.0f, aspect * 1.0f, -1.0f, 1.0f, 0.0f, 1.0f);

	// Fix up the projection matrix so it matches what Vulkan expects.
	bufData->mvp = vulkanStyleProjection(proj);

	// Select a quad based on the elapsed time.
	bufData->highlightedQuad = static_cast<int32_t>(accumulatedTime) % 10;

	// Write the type of mipmaps associated to the texture we are showing.
	bufData->mipmapType = textureIndex;

	vkUnmapMemory(pContext->getDevice(), frame.uniformBuffer.memory);

	// Draw the quads.
	vkCmdDrawIndexed(cmd, 6 * 13, 1, 0, 0, 0);

	// Complete render pass.
	vkCmdEndRenderPass(cmd);

	// Complete the command buffer.
	VK_CHECK(vkEndCommandBuffer(cmd));

	// Submit it to the queue.
	pContext->submitSwapchain(cmd);
}

void Mipmapping::termBackbuffers()
{
	// Tear down backbuffers.
	// If our swapchain changes, we will call this, and create a new swapchain.
	VkDevice device = pContext->getDevice();

	if (!backbuffers.empty())
	{
		// Wait until device is idle before teardown.
		vkQueueWaitIdle(pContext->getGraphicsQueue());
		for (auto &backbuffer : backbuffers)
		{
			vkDestroyFramebuffer(device, backbuffer.framebuffer, nullptr);
			vkDestroyImageView(device, backbuffer.view, nullptr);
		}
		backbuffers.clear();
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
}

void Mipmapping::termPerFrame()
{
	VkDevice device = pContext->getDevice();

	for (auto &frame : perFrame)
	{
		vkFreeMemory(device, frame.uniformBuffer.memory, nullptr);
		vkDestroyBuffer(device, frame.uniformBuffer.buffer, nullptr);
		vkDestroyDescriptorPool(device, frame.descriptorPool, nullptr);
	}
	perFrame.clear();
}

void Mipmapping::terminate()
{
	vkDeviceWaitIdle(pContext->getDevice());

	// Teardown.
	VkDevice device = pContext->getDevice();

	// Vertex buffer
	vkDestroyBuffer(device, vertexBuffer.buffer, nullptr);
	vkFreeMemory(device, vertexBuffer.memory, nullptr);

	// Index buffer
	vkDestroyBuffer(device, indexBuffer.buffer, nullptr);
	vkFreeMemory(device, indexBuffer.memory, nullptr);

	// Textures
	for (auto &texture : textures)
	{
		vkDestroyImageView(device, texture.view, nullptr);
		vkDestroyImage(device, texture.image, nullptr);
		vkDestroySampler(device, texture.sampler, nullptr);
		vkFreeMemory(device, texture.memory, nullptr);
	}

	vkDestroyImageView(device, labelTexture.view, nullptr);
	vkDestroyImage(device, labelTexture.image, nullptr);
	vkDestroySampler(device, labelTexture.sampler, nullptr);
	vkFreeMemory(device, labelTexture.memory, nullptr);

	// Per-frame resources
	termPerFrame();
	termBackbuffers();

	vkDestroyPipelineCache(device, pipelineCache, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

void Mipmapping::initPerFrame(unsigned numBackbuffers)
{
	VkDevice device = pContext->getDevice();

	for (unsigned i = 0; i < numBackbuffers; i++)
	{
		// Create one uniform buffer per swapchain frame. We will update the uniform buffer every frame
		// and we don't want to stomp on a uniform buffer already in flight.
		PerFrame frame;
		frame.uniformBuffer = createBuffer(nullptr, sizeof(UniformBufferData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		// Allocate descriptor set from a pool.
		static const VkDescriptorPoolSize poolSizes[2] = {
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 },
		};

		VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		poolInfo.poolSizeCount = 2;
		poolInfo.pPoolSizes = poolSizes;
		poolInfo.maxSets = 1;
		VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &frame.descriptorPool));

		VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		allocInfo.descriptorPool = frame.descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &setLayout;
		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &frame.descriptorSet));

		// Write our uniform and texture descriptors into the descriptor set.
		VkWriteDescriptorSet writes[3] = {
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
			{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
		};

		VkDescriptorBufferInfo bufferInfo = { frame.uniformBuffer.buffer, 0, sizeof(UniformBufferData) };
		VkDescriptorImageInfo imageInfo = { textures[0].sampler, textures[0].view, textures[0].layout };
		VkDescriptorImageInfo labelImageInfo = { labelTexture.sampler, labelTexture.view, labelTexture.layout };

		writes[0].dstSet = frame.descriptorSet;
		writes[0].dstBinding = 0;
		writes[0].descriptorCount = 1;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[0].pImageInfo = &imageInfo;

		writes[1].dstSet = frame.descriptorSet;
		writes[1].dstBinding = 1;
		writes[1].descriptorCount = 1;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].pImageInfo = &labelImageInfo;

		writes[2].dstSet = frame.descriptorSet;
		writes[2].dstBinding = 2;
		writes[2].descriptorCount = 1;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[2].pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);

		perFrame.push_back(frame);
	}
}

void Mipmapping::updateSwapchain(const vector<VkImage> &newBackbuffers, const Platform::SwapchainDimensions &dim)
{
	VkDevice device = pContext->getDevice();
	width = dim.width;
	height = dim.height;

	// In case we're reinitializing the swapchain, terminate the old one first.
	termBackbuffers();
	termPerFrame();

	// We can't initialize the renderpass until we know the swapchain format.
	initRenderPass(dim.format);
	// We can't initialize the pipeline until we know the render pass.
	initPipeline();

	// Initialize per-frame resources.
	initPerFrame(newBackbuffers.size());

	// For all backbuffers in the swapchain ...
	for (auto image : newBackbuffers)
	{
		Backbuffer backbuffer;
		backbuffer.image = image;

		// Create an image view which we can render into.
		VkImageViewCreateInfo view = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = dim.format;
		view.image = image;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.layerCount = 1;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.components.r = VK_COMPONENT_SWIZZLE_R;
		view.components.g = VK_COMPONENT_SWIZZLE_G;
		view.components.b = VK_COMPONENT_SWIZZLE_B;
		view.components.a = VK_COMPONENT_SWIZZLE_A;

		VK_CHECK(vkCreateImageView(device, &view, nullptr, &backbuffer.view));

		// Build the framebuffer.
		VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbInfo.renderPass = renderPass;
		fbInfo.attachmentCount = 1;
		fbInfo.pAttachments = &backbuffer.view;
		fbInfo.width = width;
		fbInfo.height = height;
		fbInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &backbuffer.framebuffer));

		backbuffers.push_back(backbuffer);
	}
}

VulkanApplication *MaliSDK::createApplication()
{
	return new Mipmapping();
}
