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
#include <random>
#include <string.h>

using namespace MaliSDK;
using namespace std;
using namespace glm;

#define NUM_PARTICLES_PER_WORKGROUP 64
#define NUM_PARTICLES (32 * 1024)

struct Backbuffer
{
	// We get this image from the platform. Its memory is bound to the display or
	// window.
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

	// Size of the buffer.
	VkDeviceSize size;
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

struct Pipeline
{
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
	VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
};

struct Vertex
{
	vec2 position;
	vec2 texCoord;
};

class BasicCompute : public VulkanApplication
{
public:
	virtual bool initialize(Context *pContext);
	virtual void render(unsigned swapchainIndex, float deltaTime);
	virtual void terminate();
	virtual void updateSwapchain(const vector<VkImage> &backbuffers, const Platform::SwapchainDimensions &dim);

private:
	Context *pContext = nullptr;

	vector<Backbuffer> backbuffers;
	unsigned width, height;

	// The renderpass description.
	VkRenderPass renderPass;

	// Pipeline objects can be cached in a pipeline cache.
	// Mostly useful when you have many pipeline objects.
	VkPipelineCache pipelineCache;

	Pipeline computePipeline;
	Pipeline drawPipeline;

	Buffer positionBuffer;
	Buffer velocityBuffer;
	Buffer colorBuffer;

	Buffer createBuffer(const void *pInitial, size_t size, VkFlags usage);

	uint32_t findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements);
	uint32_t findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements, uint32_t hostRequirements);

	void initRenderPass(VkFormat format);
	void termBackbuffers();

	void initVertexBuffers();
	void initComputePipelineLayout();
	void initComputeDescriptorSet();
	void initComputePipeline();

	void initDrawPipelineLayout();
	void initDrawPipeline();

	void destroyBuffer(Buffer *pBuffer);
	void destroyPipeline(Pipeline *pPipeline);

	void memoryBarrier(VkCommandBuffer cmd, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
	                   VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);

	float accumulatedTime = 0.0f;
};

// To create a buffer, both the device and application have requirements from
// the buffer object.
// Vulkan exposes the different types of buffers the device can allocate, and we
// have to find a suitable one.
// deviceRequirements is a bitmask expressing which memory types can be used for
// a buffer object.
// The different memory types' properties must match with what the application
// wants.
uint32_t BasicCompute::findMemoryTypeFromRequirements(uint32_t deviceRequirements, uint32_t hostRequirements)
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

uint32_t BasicCompute::findMemoryTypeFromRequirementsWithFallback(uint32_t deviceRequirements,
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

	// If we cannot find the particular memory type we're looking for, just pick
	// the first one available.
	if (hostRequirements != 0)
		return findMemoryTypeFromRequirements(deviceRequirements, 0);
	else
	{
		LOGE("Failed to obtain suitable memory type.\n");
		abort();
	}
}

void BasicCompute::memoryBarrier(VkCommandBuffer cmd, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                                 VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask)
{
	// VkMemoryBarrier is a global memory barrier.
	// It is possible to use VkBufferMemoryBarrier to have more fine-grained
	// barriers, but it won't make a difference here.
	VkMemoryBarrier barrier = { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
	barrier.srcAccessMask = srcAccessMask;
	barrier.dstAccessMask = dstAccessMask;

	vkCmdPipelineBarrier(cmd, srcStageMask, dstStageMask, false, 1, &barrier, 0, nullptr, 0, nullptr);
}

Buffer BasicCompute::createBuffer(const void *pInitialData, size_t size, VkFlags usage)
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

	// Buffers are not backed by memory, so bind our memory explicitly to the
	// buffer.
	vkBindBufferMemory(device, buffer.buffer, buffer.memory, 0);

	// Map the memory and dump data in there.
	if (pInitialData)
	{
		void *pData;
		VK_CHECK(vkMapMemory(device, buffer.memory, 0, size, 0, &pData));
		memcpy(pData, pInitialData, size);
		vkUnmapMemory(device, buffer.memory);
	}

	buffer.size = size;
	return buffer;
}

void BasicCompute::initRenderPass(VkFormat format)
{
	VkAttachmentDescription attachment = { 0 };
	// Backbuffer format.
	attachment.format = format;
	// Not multisampled.
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	// Clear the framebuffer.
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
	// actually wait for the semaphore, so we must also wait for that pipeline
	// stage.
	VkSubpassDependency dependency = { 0 };
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Since we changed the image layout, we need to make the memory visible to
	// color attachment to modify.
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

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

void BasicCompute::initVertexBuffers()
{
	vector<vec2> positions;
	vector<vec2> velocities;
	vector<vec4> colors;

	positions.reserve(NUM_PARTICLES);
	velocities.reserve(NUM_PARTICLES);
	colors.reserve(NUM_PARTICLES);

	uniform_real_distribution<float> uniform(-1.0f, 1.0f);
	mt19937 engine;

	for (unsigned i = 0; i < NUM_PARTICLES; i++)
	{
		positions.push_back(vec2(0.2f * uniform(engine), 0.2f * uniform(engine)));

		float velocity = 0.008f + 0.003f * uniform(engine);
		float angle = 100.0f * uniform(engine);

		velocities.push_back(velocity * vec2(glm::cos(angle), glm::sin(angle)));

		// Create a random color in YUV space with highly saturated colors
		// with the biggest random factor being the random hue.
		float y = 0.8f + 0.2f * uniform(engine);
		float saturation = 0.8f + 0.2f * uniform(engine);
		float hue = 100.0f * uniform(engine);

		float u = saturation * glm::cos(hue);
		float v = saturation * glm::sin(hue);
		vec3 rgb = mat3(1.0f, 1.0f, 1.0f, 0.0f, -0.39465f, 2.03211f, 1.13983f, -0.58060f, 0.0f) * vec3(y, u, v);
		colors.push_back(vec4(rgb, 0.4f));
	}

	positionBuffer = createBuffer(positions.data(), positions.size() * sizeof(vec2),
	                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	velocityBuffer = createBuffer(velocities.data(), velocities.size() * sizeof(vec2),
	                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
	colorBuffer = createBuffer(colors.data(), colors.size() * sizeof(vec4), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void BasicCompute::initDrawPipelineLayout()
{
	VkDevice device = pContext->getDevice();

	// We're not using any resources in the graphics pipeline.
	VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = 0;
	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &drawPipeline.pipelineLayout));
}

void BasicCompute::initComputePipelineLayout()
{
	VkDevice device = pContext->getDevice();

	VkDescriptorSetLayoutBinding bindings[2] = { { 0 } };
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[0].descriptorCount = 1;
	bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[1].descriptorCount = 1;
	bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

	VkDescriptorSetLayoutCreateInfo info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	info.bindingCount = 2;
	info.pBindings = bindings;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &computePipeline.setLayout));

	VkPipelineLayoutCreateInfo layoutInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &computePipeline.setLayout;
	VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &computePipeline.pipelineLayout));
}

void BasicCompute::initComputePipeline()
{
	VkDevice device = pContext->getDevice();

	initComputePipelineLayout();
	initComputeDescriptorSet();

	VkComputePipelineCreateInfo info = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
	info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	info.stage.module = loadShaderModule(device, "shaders/particle.comp.spv");
	info.stage.pName = "main";
	info.layout = computePipeline.pipelineLayout;

	VK_CHECK(vkCreateComputePipelines(device, pipelineCache, 1, &info, nullptr, &computePipeline.pipeline));

	// Pipeline is baked, we can delete the shader module now.
	vkDestroyShaderModule(device, info.stage.module, nullptr);
}

void BasicCompute::initDrawPipeline()
{
	VkDevice device = pContext->getDevice();

	initDrawPipelineLayout();

	// Specify we will use triangle strip to draw geometry.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO
	};
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

	// Specify our two attributes, Position and Color.
	VkVertexInputAttributeDescription attributes[3] = { { 0 } };
	attributes[0].location = 0;
	attributes[0].binding = 0;
	attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
	attributes[0].offset = 0;

	attributes[1].location = 1;
	attributes[1].binding = 1;
	attributes[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
	attributes[1].offset = 0;

	// We're using two separate buffers, one for vertex and one for compute.
	// The reason we split them up like this is that we are updating
	// position data every frame, but color remains constant.
	//
	// We want to tightly pack the position data in this case to avoid wasting
	// bandwidth.
	VkVertexInputBindingDescription bindings[3] = { { 0 } };
	bindings[0].binding = 0;
	bindings[0].stride = sizeof(vec2);
	bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	bindings[1].binding = 1;
	bindings[1].stride = sizeof(vec4);
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

	// Our attachment will write to all color channels, with additive blending.
	VkPipelineColorBlendAttachmentState blendAttachment = { 0 };
	blendAttachment.blendEnable = true;
	blendAttachment.colorWriteMask = 0xf;
	// Create a blend equation of src.a * src.rgb + dst.rgb.
	blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

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

	// Specify that these states will be dynamic, i.e. not part of pipeline state
	// object.
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
	shaderStages[0].module = loadShaderModule(device, "shaders/particle.vert.spv");
	shaderStages[0].pName = "main";
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].module = loadShaderModule(device, "shaders/particle.frag.spv");
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
	pipe.layout = drawPipeline.pipelineLayout;

	VK_CHECK(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipe, nullptr, &drawPipeline.pipeline));

	// Pipeline is baked, we can delete the shader modules now.
	vkDestroyShaderModule(device, shaderStages[0].module, nullptr);
	vkDestroyShaderModule(device, shaderStages[1].module, nullptr);
}

bool BasicCompute::initialize(Context *pContext)
{
	this->pContext = pContext;

	// Create the vertex buffers.
	initVertexBuffers();

	// Initialize compute pipeline.
	// We know everything up front, unlike the graphics pipeline
	// which depends on the backbuffer format which we do not know yet.
	initComputePipeline();

	// Create a pipeline cache (although we'll only create one pipeline).
	VkPipelineCacheCreateInfo pipelineCacheInfo = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
	VK_CHECK(vkCreatePipelineCache(pContext->getDevice(), &pipelineCacheInfo, nullptr, &pipelineCache));

	return true;
}

void BasicCompute::render(unsigned swapchainIndex, float)
{
	// Render to this backbuffer.
	Backbuffer &backbuffer = backbuffers[swapchainIndex];

	// Request a fresh command buffer.
	VkCommandBuffer cmd = pContext->requestPrimaryCommandBuffer();

	// We will only submit this once before it's recycled.
	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmd, &beginInfo);

	// Compute
	// First, we have to wait until previous vertex shader invocations have
	// completed
	// since we will overwrite the vertex buffer used in previous frame here.
	//
	// We only need execution barriers here and no memory barrier since we have a
	// write-after-read hazard.
	// Write-after-read only requires execution barriers.
	// We have not touched the memory written by compute earlier, so no memory
	// synchronization is needed.
	memoryBarrier(cmd, 0, 0, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

	// Bind the compute pipeline.
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline);

	// Bind descriptor set.
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipelineLayout, 0, 1,
	                        &computePipeline.descriptorSet, 0, nullptr);

	// Dispatch compute job.
	vkCmdDispatch(cmd, uint32_t(positionBuffer.size / sizeof(vec2)) / NUM_PARTICLES_PER_WORKGROUP, 1, 1);

	// Barrier between compute and vertex shading.
	// Vertex shading cannot start until we're done updating the position buffers.
	// Make sure that the vertex attributes can be read correctly by using
	// VERTEX_ATTRIBUTE_READ_BIT dstAccessMask.
	memoryBarrier(cmd, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
	              VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

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
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, drawPipeline.pipeline);

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
	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(cmd, 0, 1, &positionBuffer.buffer, &offset);
	vkCmdBindVertexBuffers(cmd, 1, 1, &colorBuffer.buffer, &offset);

	// Draw a quad with one instance.
	vkCmdDraw(cmd, uint32_t(positionBuffer.size / sizeof(vec2)), 1, 0, 0);

	// Complete render pass.
	vkCmdEndRenderPass(cmd);

	// Complete the command buffer.
	VK_CHECK(vkEndCommandBuffer(cmd));

	// Submit it to the queue.
	pContext->submitSwapchain(cmd);
}

void BasicCompute::termBackbuffers()
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

		destroyPipeline(&drawPipeline);
	}
}

void BasicCompute::destroyBuffer(Buffer *pBuffer)
{
	VkDevice device = pContext->getDevice();

	vkDestroyBuffer(device, pBuffer->buffer, nullptr);
	vkFreeMemory(device, pBuffer->memory, nullptr);

	memset(pBuffer, 0, sizeof(*pBuffer));
}

void BasicCompute::destroyPipeline(Pipeline *pPipeline)
{
	VkDevice device = pContext->getDevice();

	vkDestroyPipelineLayout(device, pPipeline->pipelineLayout, nullptr);
	vkDestroyPipeline(device, pPipeline->pipeline, nullptr);

	if (pPipeline->setLayout != VK_NULL_HANDLE)
		vkDestroyDescriptorSetLayout(device, pPipeline->setLayout, nullptr);
	if (pPipeline->descriptorPool != VK_NULL_HANDLE)
		vkDestroyDescriptorPool(device, pPipeline->descriptorPool, nullptr);

	memset(pPipeline, 0, sizeof(*pPipeline));
}

void BasicCompute::terminate()
{
	vkDeviceWaitIdle(pContext->getDevice());

	// Teardown.
	VkDevice device = pContext->getDevice();

	// Vertex buffers
	destroyBuffer(&positionBuffer);
	destroyBuffer(&velocityBuffer);
	destroyBuffer(&colorBuffer);

	// Per-frame resources
	termBackbuffers();

	destroyPipeline(&computePipeline);
	vkDestroyPipelineCache(device, pipelineCache, nullptr);
}

void BasicCompute::initComputeDescriptorSet()
{
	VkDevice device = pContext->getDevice();

	static const VkDescriptorPoolSize poolSizes[2] = {
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }, { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
	};

	VkDescriptorPoolCreateInfo poolInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.poolSizeCount = 2;
	poolInfo.pPoolSizes = poolSizes;
	poolInfo.maxSets = 1;
	VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &computePipeline.descriptorPool));

	VkDescriptorSetAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	allocInfo.descriptorPool = computePipeline.descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &computePipeline.setLayout;
	VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &computePipeline.descriptorSet));

	VkWriteDescriptorSet writes[2] = {
		{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET }, { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET },
	};

	const VkDescriptorBufferInfo bufferInfo[2] = {
		{ positionBuffer.buffer, 0, positionBuffer.size }, { velocityBuffer.buffer, 0, velocityBuffer.size },
	};

	writes[0].dstSet = computePipeline.descriptorSet;
	writes[0].dstBinding = 0;
	writes[0].descriptorCount = 1;
	writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[0].pBufferInfo = &bufferInfo[0];

	writes[1].dstSet = computePipeline.descriptorSet;
	writes[1].dstBinding = 1;
	writes[1].descriptorCount = 1;
	writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].pBufferInfo = &bufferInfo[1];

	vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
}

void BasicCompute::updateSwapchain(const vector<VkImage> &newBackbuffers, const Platform::SwapchainDimensions &dim)
{
	VkDevice device = pContext->getDevice();
	width = dim.width;
	height = dim.height;

	// In case we're reinitializing the swapchain, terminate the old one first.
	termBackbuffers();

	// We can't initialize the renderpass until we know the swapchain format.
	initRenderPass(dim.format);

	// We can't initialize the pipeline until we know the render pass.
	initDrawPipeline();

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
	return new BasicCompute();
}
