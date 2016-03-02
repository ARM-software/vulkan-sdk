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
#include <string.h>

using namespace MaliSDK;
using namespace std;
using namespace glm;

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

class SpinningCube : public VulkanApplication
{
public:
	bool initialize(Context *pContext) final;
	void render(unsigned swapchainIndex, float deltaTime) final;
	void terminate() final;
	void updateSwapchain(const vector<VkImage> &backbuffers, const Platform::SwapchainDimensions &dim) final;

private:
	static const VkFormat depthBufferFormat = VK_FORMAT_D16_UNORM;

	Context *pContext = nullptr;

	vector<Backbuffer> backbuffers;
	unsigned width, height;

	// The resources
	VkDescriptorSet descriptorSet;
	VkDescriptorPool descriptorPool;

	// The renderpass description.
	VkRenderPass renderPass;

	// The graphics pipeline.
	VkPipeline pipeline;

	// Pipeline objects can be cached in a pipeline cache.
	// Mostly useful when you have many pipeline objects.
	VkPipelineCache pipelineCache;

	// Specified the pipeline layout for resources.
	// We don't use any in this sample, but we still need to provide a dummy one.
	VkPipelineLayout pipelineLayout;

	VkDescriptorSetLayout setLayout;

	// Buffer that holds the vertex positions.
	Buffer positionBuffer;

	// Buffer that holds the texture coordinates.
	Buffer texCoordsBuffer;

	// The index buffer
	Buffer indexBuffer;

	Texture texture;

	// Memory for the depth buffer.
	VkDeviceMemory depthBufferMemory;

	// Image for the depth buffer.
	VkImage depthBufferImage;

	// Image view for the depth buffer.
	VkImageView depthBufferView;

	Buffer createBuffer(const void *pInitial, size_t size, VkFlags usage);
	Texture createTextureFromAsset(const char *pPath);
	void initializeDescriptorSets();

	uint32_t findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements);
	uint32_t findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements, uint32_t hostRequirements);

	void initRenderPass(VkFormat format);
	void termBackbuffers();

	void initVertexAndIndexBuffers();
	void initPipeline();
	void initPipelineLayout();

	void imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
	                        VkAccessFlags dstAccessMask, VkPipelineStageFlags srcStageMask,
	                        VkPipelineStageFlags dstStageMask, VkImageLayout oldLayout, VkImageLayout newLayout);

	void initDepthBuffer(unsigned width, unsigned height);

	float accumulatedTime = 0.0f;
};

// To create a buffer, both the device and application have requirements from the buffer object.
// Vulkan exposes the different types of buffers the device can allocate, and we have to find a suitable one.
// deviceRequirements is a bitmask expressing which memory types can be used for a buffer object.
// The different memory types' properties must match with what the application wants.
uint32_t SpinningCube::findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements)
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

uint32_t SpinningCube::findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements,
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

Texture SpinningCube::createTextureFromAsset(const char *pPath)
{
	// We want to first create a staging buffer.
	//
	// We will then copy this buffer into an optimally tiled texture with vkCmdCopyBufferToImage.
	// The layout of such a texture is not specified as it is highly GPU-dependent and optimized for
	// utilizing texture caches better.
	unsigned width, height;
	vector<uint8_t> buffer;

	if (FAILED(loadRgba8888TextureFromAsset(pPath, &buffer, &width, &height)))
	{
		LOGE("Failed to load texture from asset.\n");
		abort();
	}

	VkDevice device = pContext->getDevice();
	VkImage image;
	VkDeviceMemory memory;

	Buffer stagingBuffer = createBuffer(buffer.data(), width * height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

	// We will transition the actual texture into a proper layout before transfering any data, so leave it as undefined.
	VkImageCreateInfo info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	info.imageType = VK_IMAGE_TYPE_2D;
	info.format = VK_FORMAT_R8G8B8A8_UNORM;
	info.extent.width = width;
	info.extent.height = height;
	info.extent.depth = 1;
	info.mipLevels = 1;
	info.arrayLayers = 1;
	info.samples = VK_SAMPLE_COUNT_1_BIT;
	info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	info.tiling = VK_IMAGE_TILING_OPTIMAL;
	info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	// Create image.
	VK_CHECK(vkCreateImage(device, &info, nullptr, &image));

	// Allocate memory for the texture.
	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(device, image, &memReqs);

	VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	alloc.allocationSize = memReqs.size;
	// If a device local memory type exists, we should use that.
	alloc.memoryTypeIndex =
	    findMemoryTypeFromRequirementsWithFallback(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &memory));
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
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;

	VkImageView view;
	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view));

	// Now we need to transfer the staging texture into the real texture.
	// For this we will need a command buffer.
	VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Transition the uninitialized texture into a TRANSFER_DST_OPTIMAL layout.
	// We do not need to wait for anything to make the transition, so use TOP_OF_PIPE_BIT as the srcStageMask.
	imageMemoryBarrier(cmd, image, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
	                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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
	vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	// Wait for all transfers to complete before we let any fragment shading begin.
	imageMemoryBarrier(cmd, image, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
	                   VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
	                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	VK_CHECK(vkEndCommandBuffer(cmd));
	pContext->submit(cmd);

	// We want to free the staging buffer and memory right away, so wait for GPU complete the transfer.
	vkQueueWaitIdle(pContext->getGraphicsQueue());

	// Now it's safe to free the temporary resources.
	vkDestroyBuffer(device, stagingBuffer.buffer, nullptr);
	vkFreeMemory(device, stagingBuffer.memory, nullptr);

	// Finally, create a sampler.
	VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.compareEnable = false;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	VkSampler sampler;
	VK_CHECK(vkCreateSampler(device, &samplerInfo, nullptr, &sampler));

	Texture ret = {
		image, view, memory, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, sampler, width, height,
	};
	return ret;
}

void SpinningCube::imageMemoryBarrier(VkCommandBuffer cmd, VkImage image, VkAccessFlags srcAccessMask,
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
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, false, 0, nullptr, 0, nullptr, 1, &barrier);
}

Buffer SpinningCube::createBuffer(const void *pInitialData, size_t size, VkFlags usage)
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

void SpinningCube::initRenderPass(VkFormat format)
{
	VkAttachmentDescription attachments[2] = { { 0 } };

	// Setup the color attachment.

	// Color backbuffer format.
	attachments[0].format = format;
	// Not multisampled.
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	// When starting the frame, we want tiles to be cleared.
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// When ending the frame, we want tiles to be written out.
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	// Don't care about stencil since we're not using it.
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// Setup the depth attachment.
	attachments[1] = attachments[0];
	attachments[1].format = depthBufferFormat;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// We have one subpass.
	// This subpass has 1 color attachment and 1 depth.
	VkAttachmentReference colorRef = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthRef = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkSubpassDescription subpass = { 0 };
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;
	subpass.pDepthStencilAttachment = &depthRef;

	// Create a dependency to external events.
	// We need to wait for the WSI semaphore to signal.
	// Only pipeline stages which depend on COLOR_ATTACHMENT_OUTPUT_BIT will
	// actually wait for the semaphore, so we must also wait for that pipeline stage.
	//
	// We are also using a depth buffer here which is reused every frame.
	// We need to wait for earlier depth buffer accesses to complete as well.
	// Late and early fragment test stages cover this.
	VkSubpassDependency dependency = { 0 };
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
	                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	// We are creating a write-after-read dependency (presentation must be done reading), so we don't need a memory barrier.
	// We do need a memory barrier for the depth buffer access however.
	dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstAccessMask =
	    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

	// Finally, create the renderpass.
	VkRenderPassCreateInfo rpInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
	rpInfo.attachmentCount = 2;
	rpInfo.pAttachments = attachments;
	rpInfo.subpassCount = 1;
	rpInfo.pSubpasses = &subpass;
	rpInfo.dependencyCount = 1;
	rpInfo.pDependencies = &dependency;
	VK_CHECK(vkCreateRenderPass(pContext->getDevice(), &rpInfo, nullptr, &renderPass));
}

void SpinningCube::initVertexAndIndexBuffers()
{
	static const vec3 positionData[] = {
		vec3(1.0f, -1.0f, -1.0f),  vec3(1.0f, -1.0f, 1.0f),  vec3(-1.0f, -1.0f, 1.0f),  vec3(1.0f, -1.0f, -1.0f),
		vec3(-1.0f, -1.0f, -1.0f), vec3(-1.0f, 1.0f, -1.0f), vec3(-1.0f, 1.0f, -1.0f),  vec3(-1.0f, 1.0f, 1.0f),
		vec3(1.0f, 1.0f, 1.0f),    vec3(1.0f, 1.0f, -1.0f),  vec3(1.0f, 1.0f, 1.0f),    vec3(1.0f, -1.0f, 1.0f),
		vec3(1.0f, 1.0f, 1.0f),    vec3(-1.0f, 1.0f, 1.0f),  vec3(-1.0f, -1.0f, 1.0f),  vec3(-1.0f, -1.0f, 1.0f),
		vec3(-1.0f, 1.0f, 1.0f),   vec3(-1.0f, 1.0f, -1.0f), vec3(-1.0f, -1.0f, -1.0f), vec3(1.0f, 1.0f, -1.0f),
		vec3(1.0f, 1.0f, -1.0f),   vec3(1.0f, -1.0f, -1.0f), vec3(1.0f, -1.0f, 1.0f),   vec3(-1.0f, -1.0f, -1.0f)
	};

	positionBuffer = createBuffer(positionData, sizeof(positionData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	static const vec2 texcData[] = { vec2(1.0f, 1.0f), vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 0.0f),
		                             vec2(0.0f, 1.0f), vec2(1.0f, 1.0f), vec2(1.0f, 0.0f), vec2(0.0f, 0.0f),
		                             vec2(0.0f, 1.0f), vec2(1.0f, 1.0f), vec2(1.0f, 0.0f), vec2(0.0f, 0.0f),
		                             vec2(1.0f, 1.0f), vec2(1.0f, 0.0f), vec2(0.0f, 0.0f), vec2(0.0f, 1.0f),
		                             vec2(1.0f, 1.0f), vec2(1.0f, 0.0f), vec2(0.0f, 1.0f), vec2(1.0f, 0.0f),
		                             vec2(1.0f, 1.0f), vec2(0.0f, 1.0f), vec2(0.0f, 1.0f), vec2(0.0f, 0.0f) };

	texCoordsBuffer = createBuffer(texcData, sizeof(texcData), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	static const uint16_t indexData[] = { 0, 1,  2,  18, 0, 2,  3,  4,  5,  19, 3,  5,  6,  7,  8,  20, 6,  8,
		                                  9, 10, 11, 21, 9, 11, 12, 13, 14, 22, 12, 14, 15, 16, 17, 23, 15, 17 };
	indexBuffer = createBuffer(indexData, sizeof(indexData), VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void SpinningCube::initPipelineLayout()
{
	VkDevice device = pContext->getDevice();

	VkDescriptorSetLayoutBinding bindings[1] = { { 0 } };
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	info.bindingCount = 1;
	info.pBindings = bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &setLayout));

	VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &setLayout;

	// Setup the push constants. It's a single mat4 in the vertex shader.
	VkPushConstantRange pushConstantInfo = { 0 };
	pushConstantInfo.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantInfo.offset = 0;
	pushConstantInfo.size = sizeof(mat4);
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantInfo;

	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout));
}

void SpinningCube::initPipeline()
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
	attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributes[0].offset = 0;
	attributes[1].location = 1; // TexCoord in shader specifies layout(location = 1) to link with this attribute.
	attributes[1].binding = 1;
	attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[1].offset = 0;

	// We have two vertex buffers. The position buffer with stride sizeof(float)*3 and the tex coord buffer with stride
	// sizeof(float)*2
	VkVertexInputBindingDescription bindings[2] = { { 0 } };
	bindings[0].binding = 0;
	bindings[0].stride = sizeof(float) * 3;
	bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
	bindings[1].binding = 1;
	bindings[1].stride = sizeof(float) * 2;
	bindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vertexInput = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	vertexInput.vertexBindingDescriptionCount = 2;
	vertexInput.pVertexBindingDescriptions = bindings;
	vertexInput.vertexAttributeDescriptionCount = 2;
	vertexInput.pVertexAttributeDescriptions = attributes;

	// Specify rasterization state.
	VkPipelineRasterizationStateCreateInfo raster = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	raster.polygonMode = VK_POLYGON_MODE_FILL;
	raster.cullMode = VK_CULL_MODE_BACK_BIT;
	raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	raster.depthClampEnable = false;
	raster.rasterizerDiscardEnable = false;
	raster.depthBiasEnable = false;
	raster.lineWidth = 1.0f;

	// Our attachment will write to all color channels, but no blending is enabled.
	VkPipelineColorBlendAttachmentState blendAttachment = { 0 };
	blendAttachment.blendEnable = false;
	blendAttachment.colorWriteMask = 0xf;

	VkPipelineColorBlendStateCreateInfo blend = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	blend.attachmentCount = 1;
	blend.pAttachments = &blendAttachment;

	// We will have one viewport and scissor box.
	VkPipelineViewportStateCreateInfo viewport = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewport.viewportCount = 1;
	viewport.scissorCount = 1;

	// Enable all depth testing.
	VkPipelineDepthStencilStateCreateInfo depthStencil = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	depthStencil.depthTestEnable = true;
	depthStencil.depthWriteEnable = true;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
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

void SpinningCube::initializeDescriptorSets()
{
	VkDevice device = pContext->getDevice();

	static const VkDescriptorPoolSize poolSizes[1] = {
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
	};

	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 1;
	VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool));

	VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &setLayout;
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

	VkWriteDescriptorSet writes[1] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
	};

	VkDescriptorImageInfo imageInfo = { texture.sampler, texture.view, texture.layout };

	writes[0].dstSet = descriptorSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writes[0].pImageInfo = &imageInfo;

	vkUpdateDescriptorSets(device, 1, writes, 0, nullptr);
}

bool SpinningCube::initialize(Context *pContext)
{
	this->pContext = pContext;
	VkDevice device = pContext->getDevice();

	// Create the vertex buffer.
	initVertexAndIndexBuffers();

	// Initialize the pipeline layout.
	initPipelineLayout();

	// Load texture.
	texture = createTextureFromAsset("textures/icon.png");

	// Create a pipeline cache (although we'll only create one pipeline).
	VkPipelineCacheCreateInfo pipelineCacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	VK_CHECK(vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &pipelineCache));

	// Initialize the descriptor set
	initializeDescriptorSets();

	return true;
}

void SpinningCube::render(unsigned swapchainIndex, float deltaTime)
{
	// Render to this backbuffer.
	Backbuffer &backbuffer = backbuffers[swapchainIndex];

	// Request a fresh command buffer.
	VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Set clear color and clear depth values. Since we have 2 attachments we need 2 VkClearValue.
	VkClearValue clearValues[2] = { 0 };
	clearValues[0].color.float32[0] = 0.1f;
	clearValues[0].color.float32[1] = 0.1f;
	clearValues[0].color.float32[2] = 0.2f;
	clearValues[0].color.float32[3] = 1.0f;
	clearValues[1].depthStencil.depth = 1.0f;

	// Begin the render pass.
	VkRenderPassBeginInfo rpBegin = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
	rpBegin.renderPass = renderPass;
	rpBegin.framebuffer = backbuffer.framebuffer;
	rpBegin.renderArea.extent.width = width;
	rpBegin.renderArea.extent.height = height;
	rpBegin.clearValueCount = 2;
	rpBegin.pClearValues = clearValues;
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

	// Bind vertex buffers.
	VkDeviceSize offsets[2] = { 0, 0 };
	VkBuffer buffers[2] = { positionBuffer.buffer, texCoordsBuffer.buffer };
	vkCmdBindVertexBuffers(cmd, 0, 2, buffers, offsets);

	// Bind the index buffer.
	vkCmdBindIndexBuffer(cmd, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT16);

	// Bind the descriptor set.
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

	// Update the push constants.
	float aspect = float(width) / height;

	// Simple perspective projection.
	mat4 proj = perspective(radians(80.0f), aspect, 0.1f, 100.0f);

	// Set the view matrix.
	mat4 view = lookAt(vec3(0.0, 0.0, 3.0), vec3(0.0), vec3(0.0, 1.0, 0.0));

	// Create a simple rotation matrix which rotates around all axes.
	accumulatedTime += deltaTime;
	mat4 model = rotate(accumulatedTime, vec3(0.0f, 0.0f, 1.0f)) *
	             rotate(accumulatedTime / 4.0f, vec3(0.0f, 1.0f, 0.0f)) *
	             rotate(accumulatedTime / 2.0f, vec3(1.0f, 0.0f, 0.0f));

	// Fix up the projection matrix so it matches what Vulkan expects.
	mat4 matrix = vulkanStyleProjection(proj) * view * model;

	vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrix), &matrix);

	// Draw the cube with one instance.
	vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

	// Complete render pass.
	vkCmdEndRenderPass(cmd);

	// Complete the command buffer.
	VK_CHECK(vkEndCommandBuffer(cmd));

	// Submit it to the queue.
	pContext->submitSwapchain(cmd);
}

void SpinningCube::termBackbuffers()
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
			vkDestroyImageView(device, backbuffer.view, nullptr);

			vkDestroyFramebuffer(device, backbuffer.framebuffer, nullptr);
		}
		backbuffers.clear();
		vkDestroyRenderPass(device, renderPass, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);

		// Depth buffer
		vkDestroyImageView(device, depthBufferView, nullptr);
		vkDestroyImage(device, depthBufferImage, nullptr);
		vkFreeMemory(device, depthBufferMemory, nullptr);
	}
}

void SpinningCube::terminate()
{
	vkDeviceWaitIdle(pContext->getDevice());

	// Teardown.
	VkDevice device = pContext->getDevice();

	// Position buffer.
	vkDestroyBuffer(device, positionBuffer.buffer, nullptr);
	vkFreeMemory(device, positionBuffer.memory, nullptr);

	// Texture coordinate buffer.
	vkDestroyBuffer(device, texCoordsBuffer.buffer, nullptr);
	vkFreeMemory(device, texCoordsBuffer.memory, nullptr);

	// Index buffer.
	vkDestroyBuffer(device, indexBuffer.buffer, nullptr);
	vkFreeMemory(device, indexBuffer.memory, nullptr);

	// Texture.
	vkDestroyImageView(device, texture.view, nullptr);
	vkDestroyImage(device, texture.image, nullptr);
	vkDestroySampler(device, texture.sampler, nullptr);
	vkFreeMemory(device, texture.memory, nullptr);

	// Resources
	vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	termBackbuffers();

	vkDestroyPipelineCache(device, pipelineCache, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
}

void SpinningCube::updateSwapchain(const vector<VkImage> &newBackbuffers, const Platform::SwapchainDimensions &dim)
{
	VkDevice device = pContext->getDevice();
	width = dim.width;
	height = dim.height;

	// In case we're reinitializing the swapchain, terminate the old one first.
	termBackbuffers();

	// We can't initialize the renderpass until we know the swapchain format.
	initRenderPass(dim.format);
	// We can't initialize the pipeline until we know the render pass.
	initPipeline();

	// Depth buffer.
	initDepthBuffer(width, height);

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
		VkImageView attachments[2] = { backbuffer.view, depthBufferView };
		VkFramebufferCreateInfo fbInfo = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		fbInfo.renderPass = renderPass;
		fbInfo.attachmentCount = 2;
		fbInfo.pAttachments = attachments;
		fbInfo.width = width;
		fbInfo.height = height;
		fbInfo.layers = 1;

		VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr, &backbuffer.framebuffer));

		backbuffers.push_back(backbuffer);
	}
}

void SpinningCube::initDepthBuffer(unsigned width, unsigned height)
{
	VkDevice device = pContext->getDevice();

	// Create the image for the depth buffer. Note that imageInfo.usage includes the
	// VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT flag. This flag allows lazy allocation of the depth buffer.
	// For Mali GPU this flag will allow the driver to never allocate memory for the depth buffer and use the
	// on-chip tile buffer instead.
	VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = depthBufferFormat;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VK_CHECK(vkCreateImage(device, &imageInfo, nullptr, &depthBufferImage));

	// Allocate memory for the depth image. Prefer a memory type with lazily allocation support.
	VkMemoryRequirements memoryRequirements = { 0 };
	vkGetImageMemoryRequirements(device, depthBufferImage, &memoryRequirements);
	uint32_t memoryTypeIndex = findMemoryTypeFromRequirementsWithFallback(memoryRequirements.memoryTypeBits,
	                                                                      VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT);

	VkMemoryAllocateInfo memInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	memInfo.allocationSize = memoryRequirements.size;
	memInfo.memoryTypeIndex = memoryTypeIndex;

	VK_CHECK(vkAllocateMemory(device, &memInfo, nullptr, &depthBufferMemory));
	VK_CHECK(vkBindImageMemory(device, depthBufferImage, depthBufferMemory, 0));

	// Create the depth buffer image.
	VkImageViewCreateInfo viewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	viewInfo.image = depthBufferImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = depthBufferFormat;
	viewInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	viewInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	viewInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	viewInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &depthBufferView));
}

VulkanApplication *MaliSDK::createApplication()
{
	return new SpinningCube();
}
