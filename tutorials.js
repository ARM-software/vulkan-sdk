var tutorials =
[
    [ "Overview of Vulkan", "vulkan_intro.html", [
      [ "Introduction", "vulkan_intro.html#vulkanIntroduction", [
        [ "Who Will Benefit from Using Vulkan?", "vulkan_intro.html#vulkanForWhom", null ]
      ] ],
      [ "Reducing CPU Overhead and Power Consumption", "vulkan_intro.html#vulkanPowerConsumption", [
        [ "Greatly Improved Multithreading", "vulkan_intro.html#vulkanMultithreading", null ],
        [ "Specifying Intended Usage Up Front", "vulkan_intro.html#vulkanUpFront", null ]
      ] ],
      [ "Command Buffers and Queues", "vulkan_intro.html#vulkanCommandBuffer", null ],
      [ "Portability", "vulkan_intro.html#vulkanPortability", [
        [ "Vulkan Window System Integration (WSI)", "vulkan_intro.html#vulkanWSI", null ]
      ] ],
      [ "Streamlined and Predictable Behavior", "vulkan_intro.html#vulkanStreamline", null ],
      [ "Layered Architecture", "vulkan_intro.html#vulkanLayers", null ],
      [ "SPIR-V Shading Language Intermediate Representation", "vulkan_intro.html#vulkanSPIRV", [
        [ "Vulkan GLSL, the Reference Language Targeting SPIR-V", "vulkan_intro.html#vulkanGLSL", null ]
      ] ],
      [ "Links", "vulkan_intro.html#vulkanLinks", null ]
    ] ],
    [ "Introduction to Vulkan on Android", "creating_vulkan_window.html", [
      [ "The Native Activity", "creating_vulkan_window.html#creatingVulkanWindowNative", null ],
      [ "Bringing up Vulkan Instance and Device", "creating_vulkan_window.html#creatingVulkanContextBringup", null ],
      [ "Creating a Vulkan Swapchain", "creating_vulkan_window.html#creatingVulkanSwapchain", null ],
      [ "Rendering to Screen", "creating_vulkan_window.html#creatingVulkanSwapchainScreen", null ],
      [ "Running the Main Loop in android_main", "creating_vulkan_window.html#creatingVulkanMainLoop", null ],
      [ "Dealing with Out-of-Date Swapchains", "creating_vulkan_window.html#creatingVulkanSwapchainDealing", null ]
    ] ],
    [ "Using Validation Layers for Debugging Applications", "_validation_layer.html", [
      [ "Introduction", "_validation_layer.html#VLIntro", [
        [ "The Loader", "_validation_layer.html#VLLoader", null ],
        [ "The Layer", "_validation_layer.html#VLLayers", null ],
        [ "The Installable Client Driver", "_validation_layer.html#VLDriver", null ]
      ] ],
      [ "Bundling Validation Layers in the Android APK", "_validation_layer.html#VLBundling", null ],
      [ "Enabling Validation Layers", "_validation_layer.html#VLEnabling", null ],
      [ "Enabling Debug Callback", "_validation_layer.html#VLDebugCallback", null ],
      [ "Links", "_validation_layer.html#VKLink", null ]
    ] ],
    [ "Hello Triangle", "hello_triangle.html", [
      [ "Introduction", "hello_triangle.html#helloTriangleIntro", [
        [ "Structure of Samples", "hello_triangle.html#helloTriangleSample", null ]
      ] ],
      [ "Initialization", "hello_triangle.html#helloTriangleInitialize", [
        [ "Top-left Always", "hello_triangle.html#helloTriangleClipSpace", null ],
        [ "Creating a Buffer", "hello_triangle.html#helloTriangleBuffer", null ],
        [ "Creating the Renderpass", "hello_triangle.html#helloTriangleRenderPass", [
          [ "Creating the Subpass", "hello_triangle.html#helloTriangleSubPass", null ],
          [ "Image Layout Transitions", "hello_triangle.html#helloTriangleTransition", null ],
          [ "Adding Subpass Dependency", "hello_triangle.html#helloTriangleExternalSubpass", null ]
        ] ],
        [ "Creating the Pipeline", "hello_triangle.html#helloTrianglePipeline", [
          [ "Using SPIR-V Shaders", "hello_triangle.html#helloTriangleShaders", null ]
        ] ],
        [ "Building VkImageView and VkFramebuffer for Swapchain Images", "hello_triangle.html#helloTriangleFramebuffer", null ]
      ] ],
      [ "Rendering a Frame", "hello_triangle.html#helloTriangleRenderLoop", [
        [ "Requesting Command Buffer", "hello_triangle.html#helloTriangleRequest", null ],
        [ "Asynchronous GPU", "hello_triangle.html#helloTriangleAsync", null ],
        [ "Beginning the Renderpass", "hello_triangle.html#helloTriangleBeginRenderPass", null ],
        [ "Using Fences to Keep Track of GPU Progress", "hello_triangle.html#helloTriangleFences", null ]
      ] ]
    ] ],
    [ "Rotating Texture", "rotating_texture.html", [
      [ "Introduction", "rotating_texture.html#rotatingTextureIntroduction", null ],
      [ "Descriptor Sets", "rotating_texture.html#rotatingTextureDescriptorSets", null ],
      [ "Uploading Textures", "rotating_texture.html#rotatingTextureUpload", null ],
      [ "Using Image Memory Barriers to Change Layouts", "rotating_texture.html#rotatingTextureImageBarrier", null ],
      [ "Creating a Pipeline Layout", "rotating_texture.html#rotatingTexturePipelineLayout", null ],
      [ "Creating a Descriptor Pool and Descriptor Sets", "rotating_texture.html#rotatingTextureDescriptorPool", null ],
      [ "Rendering", "rotating_texture.html#rotatingTextureRender", null ]
    ] ],
    [ "Multithreading in Vulkan", "multithreading.html", [
      [ "Introduction", "multithreading.html#multithreadingIntro", null ],
      [ "Rules for Safe Multithreading", "multithreading.html#multithreadingCommandBufferSafety", null ],
      [ "Secondary Command Buffers", "multithreading.html#multithreadingSecondary", null ],
      [ "Rendering", "multithreading.html#multithreadingRendering", null ]
    ] ],
    [ "Introduction to Compute Shaders in Vulkan", "basic_compute.html", [
      [ "Introduction", "basic_compute.html#basicComputeIntroduction", null ],
      [ "The Compute Pipeline", "basic_compute.html#basicComputePipeline", null ],
      [ "The Graphics Pipeline", "basic_compute.html#basicComputeDrawPipe", null ],
      [ "Rendering", "basic_compute.html#basicComputeRender", null ],
      [ "Generating the Initial Data", "basic_compute.html#basicComputeGenerate", null ]
    ] ],
    [ "Multisampling in Vulkan", "multisampling.html", [
      [ "Introduction", "multisampling.html#multisamplingIntro", [
        [ "Rendering to Multisampled Texture, Resolving Later (slow)", "multisampling.html#multisamplingSlowResolve", null ],
        [ "Resolving a transient multisampled texture to non-multisampled texture (optimal)", "multisampling.html#multisamplingResolveOnTile", null ]
      ] ],
      [ "Setting up the VkRenderpass", "multisampling.html#multisamplingRenderPass", null ],
      [ "Setting up the VkPipeline", "multisampling.html#multisamplingPipeline", null ],
      [ "Setting up the VkFramebuffers", "multisampling.html#multisamplingFramebuffer", null ],
      [ "Creating a Transient, Lazily Allocated Texture", "multisampling.html#multisamplingTexture", null ]
    ] ],
    [ "Spinning Cube with Depth Testing and Push Constants", "spinning_cube.html", [
      [ "Introduction", "spinning_cube.html#Introduction", null ],
      [ "Push constants", "spinning_cube.html#spinning_cube_pushc", [
        [ "Push constants in GLSL", "spinning_cube.html#spinning_cube_pushc_glsl", null ],
        [ "Creating a pipeline layout with push constant support", "spinning_cube.html#spinning_cube_pushc_pplinel", null ],
        [ "Updating push constants", "spinning_cube.html#spinning_cube_pushc_update", null ]
      ] ],
      [ "Depth testing", "spinning_cube.html#spinning_cube_depth", [
        [ "Creating the depth buffer", "spinning_cube.html#spinning_cube_depth_buff", null ],
        [ "Creating the framebuffer", "spinning_cube.html#spinning_cube_depth_fb", null ],
        [ "Creating the render pass", "spinning_cube.html#spinning_cube_depth_rp", null ],
        [ "Creating the pipeline", "spinning_cube.html#spinning_cube_depth_ppline", null ]
      ] ]
    ] ],
    [ "Deferring shading with Multipass", "multipass.html", [
      [ "Introduction", "multipass.html#multipassIntro", [
        [ "The Render Pass", "multipass.html#multipassRenderPass", [
          [ "The subpass dependency", "multipass.html#multipassDependency", null ]
        ] ]
      ] ],
      [ "Deferred Lighting Technique", "multipass.html#Basic", null ],
      [ "Using lazily allocated G-Buffer", "multipass.html#multipassTransient", null ],
      [ "The G-Buffer pipeline", "multipass.html#multipassGBufferPipeline", null ],
      [ "The Lighting Pipeline", "multipass.html#multipassLightPipeline", null ],
      [ "Reading Input Attachments in Vulkan GLSL", "multipass.html#multipassSubpassLoad", [
        [ "Binding input attachments to descriptor sets", "multipass.html#multipassSubpassLoadDescriptorSet", null ]
      ] ],
      [ "Nifty Debug Output", "multipass.html#multipassDebug", null ],
      [ "Mipmapping", "multipass.html#multipassMipmapping", null ],
      [ "Render Loop with multiple subpasses", "multipass.html#multipassRenderLoop", null ]
    ] ],
    [ "Adaptive Scalable Texture Compression (ASTC) with ARM Mali", "_a_s_t_c.html", [
      [ "Introduction", "_a_s_t_c.html#ASTCIntroduction", null ],
      [ "Encoding images to ASTC", "_a_s_t_c.html#ASTCEncoding", null ],
      [ "Detecting ASTC support", "_a_s_t_c.html#ASTCDetection", null ],
      [ "Uploading ASTC textures to GPU", "_a_s_t_c.html#ASTCUpload", null ],
      [ "Links", "_a_s_t_c.html#ASTCLinks", null ]
    ] ],
    [ "Mipmapping in Vulkan", "mipmapping.html", [
      [ "Introduction", "mipmapping.html#mipmappingIntroduction", null ],
      [ "Loading a mipmapped texture from pre-scaled images", "mipmapping.html#mipmappingLoadingMipmappedTexture", null ],
      [ "Generating mipmaps in Vulkan", "mipmapping.html#mipmappingGeneratingMipmaps", null ]
    ] ]
];