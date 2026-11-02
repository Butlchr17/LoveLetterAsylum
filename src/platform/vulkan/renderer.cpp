#include "renderer.hpp"

#include "shaders.hpp"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace lla {
namespace {

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

void check(VkResult result, const char* action) {
    if (result != VK_SUCCESS) {
        fail(std::string(action) + " failed (" + std::to_string(static_cast<int>(result)) + ")");
    }
}

struct XcbSurfaceCreateInfo {
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    void* connection;
    std::uint32_t window;
};

static_assert(sizeof(XcbSurfaceCreateInfo) == 40);
static_assert(offsetof(XcbSurfaceCreateInfo, connection) == 24);
static_assert(offsetof(XcbSurfaceCreateInfo, window) == 32);

using PFN_create_xcb_surface = VkResult (*)(VkInstance, const XcbSurfaceCreateInfo*, const VkAllocationCallbacks*, VkSurfaceKHR*);

struct Vertex {
    float x;
    float y;
    float r;
    float g;
    float b;
};

struct PushConstants {
    float width;
    float height;
};

class RectCanvas final : public ICanvas {
public:
    void begin(int width, int height) {
        width_ = width;
        height_ = height;
        commands_.clear();
    }

    int width() const override { return width_; }
    int height() const override { return height_; }

    void clear(Color color) override { clear_ = color; }

    void fill_rect(Rect rect, Color color) override {
        if (rect.w <= 0.f || rect.h <= 0.f) {
            return;
        }
        commands_.push_back({rect, color});
    }

    Color clear_{16, 16, 20, 255};
    int width_ = 0;
    int height_ = 0;

    struct Command {
        Rect rect;
        Color color;
    };
    std::vector<Command> commands_;
};

template <typename T>
T load_instance_proc(VkInstance instance, const char* name) {
    auto* address = vkGetInstanceProcAddr(instance, name);
    if (address == nullptr) {
        fail(std::string("Vulkan loader is missing ") + name);
    }
    return reinterpret_cast<T>(address);
}

template <typename T>
T load_device_proc(VkDevice device, const char* name) {
    auto* address = vkGetDeviceProcAddr(device, name);
    if (address == nullptr) {
        fail(std::string("Vulkan device is missing ") + name);
    }
    return reinterpret_cast<T>(address);
}

bool has_extension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    for (const VkExtensionProperties& extension : extensions) {
        if (std::strcmp(extension.extensionName, name) == 0) {
            return true;
        }
    }
    return false;
}

std::uint32_t find_memory_type(VkPhysicalDevice gpu, std::uint32_t type_bits, VkMemoryPropertyFlags required) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &properties);
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
        const bool type_matches = (type_bits & (1u << index)) != 0;
        const bool properties_match = (properties.memoryTypes[index].propertyFlags & required) == required;
        if (type_matches && properties_match) {
            return index;
        }
    }
    fail("No host-visible Vulkan memory type is available");
}

int device_score(VkPhysicalDeviceType type) {
    switch (type) {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return 5;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return 4;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return 3;
        case VK_PHYSICAL_DEVICE_TYPE_OTHER:
            return 2;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return 1;
        default:
            return 0;
    }
}

}  // namespace

struct Renderer::Impl {
    RectCanvas canvas;
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice gpu = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queue_family = 0;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D extent{};
    int window_width = 0;
    int window_height = 0;
    bool dirty = false;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer command_buffer = VK_NULL_HANDLE;
    VkSemaphore image_available = VK_NULL_HANDLE;
    VkSemaphore render_finished = VK_NULL_HANDLE;
    VkFence frame_fence = VK_NULL_HANDLE;
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory vertex_memory = VK_NULL_HANDLE;
    void* vertex_mapped = nullptr;
    VkDeviceSize vertex_capacity = 0;

    PFN_vkDestroySurfaceKHR destroy_surface = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR surface_support = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR surface_capabilities = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR surface_formats = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR surface_present_modes = nullptr;
    PFN_vkCreateSwapchainKHR create_swapchain = nullptr;
    PFN_vkDestroySwapchainKHR destroy_swapchain = nullptr;
    PFN_vkGetSwapchainImagesKHR get_swapchain_images = nullptr;
    PFN_vkAcquireNextImageKHR acquire_next_image = nullptr;
    PFN_vkQueuePresentKHR queue_present = nullptr;

    ~Impl() { shutdown(); }

    void shutdown() {
        if (device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(device);
        }
        destroy_vertex_buffer();
        destroy_swapchain_views();
        if (swapchain != VK_NULL_HANDLE) {
            destroy_swapchain(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
        }
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        if (pipeline_layout != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
            pipeline_layout = VK_NULL_HANDLE;
        }
        destroy_sync();
        if (frame_fence != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
            vkDestroyFence(device, frame_fence, nullptr);
            frame_fence = VK_NULL_HANDLE;
        }
        if (command_pool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(device, command_pool, nullptr);
            command_pool = VK_NULL_HANDLE;
            command_buffer = VK_NULL_HANDLE;
        }
        if (device != VK_NULL_HANDLE) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (surface != VK_NULL_HANDLE) {
            destroy_surface(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
        if (instance != VK_NULL_HANDLE) {
            vkDestroyInstance(instance, nullptr);
            instance = VK_NULL_HANDLE;
        }
    }

    void init(XcbWindow& window) {
        create_instance();
        create_surface(window);
        choose_device();
        create_logical_device();
        load_device_functions();
        choose_format();
        create_pipeline();
        create_commands();
        create_sync();
        ensure_vertex_capacity(6 * 4096);
        window_width = window.width();
        window_height = window.height();
        recreate_swapchain();
    }

    void create_instance() {
        std::uint32_t count = 0;
        check(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr), "Enumerating Vulkan instance extensions");
        std::vector<VkExtensionProperties> extensions(count);
        check(vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()), "Reading Vulkan instance extensions");
        constexpr const char* kXcbSurface = "VK_KHR_xcb_surface";
        if (!has_extension(extensions, VK_KHR_SURFACE_EXTENSION_NAME) || !has_extension(extensions, kXcbSurface)) {
            fail("This Vulkan loader has no XCB surface extension");
        }

        const char* enabled[] = {VK_KHR_SURFACE_EXTENSION_NAME, kXcbSurface};
        VkApplicationInfo application{};
        application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        application.pApplicationName = "Love Letter Asylum";
        application.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        application.pEngineName = "lla";
        application.engineVersion = VK_MAKE_VERSION(0, 1, 0);
        application.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        info.pApplicationInfo = &application;
        info.enabledExtensionCount = 2;
        info.ppEnabledExtensionNames = enabled;
        check(vkCreateInstance(&info, nullptr, &instance), "Creating the Vulkan instance");

        destroy_surface = load_instance_proc<PFN_vkDestroySurfaceKHR>(instance, "vkDestroySurfaceKHR");
        surface_support = load_instance_proc<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(instance, "vkGetPhysicalDeviceSurfaceSupportKHR");
        surface_capabilities = load_instance_proc<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        surface_formats = load_instance_proc<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(instance, "vkGetPhysicalDeviceSurfaceFormatsKHR");
        surface_present_modes = load_instance_proc<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(instance, "vkGetPhysicalDeviceSurfacePresentModesKHR");
    }

    void create_surface(XcbWindow& window) {
        auto create = load_instance_proc<PFN_create_xcb_surface>(instance, "vkCreateXcbSurfaceKHR");
        XcbSurfaceCreateInfo info{};
        info.sType = static_cast<VkStructureType>(1000005000);
        info.connection = window.connection();
        info.window = window.window_id();
        check(create(instance, &info, nullptr, &surface), "Creating the Vulkan XCB surface");
    }

    void choose_device() {
        std::uint32_t count = 0;
        check(vkEnumeratePhysicalDevices(instance, &count, nullptr), "Enumerating Vulkan devices");
        if (count == 0) {
            fail("No Vulkan physical device is available");
        }
        std::vector<VkPhysicalDevice> devices(count);
        check(vkEnumeratePhysicalDevices(instance, &count, devices.data()), "Reading Vulkan devices");

        int best_score = -1;
        std::string report;
        for (VkPhysicalDevice candidate : devices) {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(candidate, &properties);
            report += std::string(properties.deviceName) + "\n";
            if (properties.apiVersion < VK_API_VERSION_1_3) {
                continue;
            }

            VkPhysicalDeviceVulkan13Features features13{};
            features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &features13;
            vkGetPhysicalDeviceFeatures2(candidate, &features2);
            if (features13.dynamicRendering != VK_TRUE) {
                continue;
            }

            std::uint32_t extension_count = 0;
            vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(candidate, nullptr, &extension_count, extensions.data());
            if (!has_extension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                continue;
            }

            std::uint32_t family_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, nullptr);
            std::vector<VkQueueFamilyProperties> families(family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &family_count, families.data());
            bool found_family = false;
            std::uint32_t family = 0;
            for (std::uint32_t index = 0; index < family_count; ++index) {
                if ((families[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
                    continue;
                }
                VkBool32 can_present = VK_FALSE;
                surface_support(candidate, index, surface, &can_present);
                if (can_present == VK_TRUE) {
                    family = index;
                    found_family = true;
                    break;
                }
            }
            if (!found_family) {
                continue;
            }

            const int score = device_score(properties.deviceType);
            if (score > best_score) {
                best_score = score;
                gpu = candidate;
                queue_family = family;
            }
        }
        if (gpu == VK_NULL_HANDLE) {
            fail("No Vulkan 1.3 device can present this window.\n" + report);
        }

        VkPhysicalDeviceProperties chosen{};
        vkGetPhysicalDeviceProperties(gpu, &chosen);
        std::cout << "Vulkan device: " << chosen.deviceName << '\n' << std::flush;
    }

    void create_logical_device() {
        const float priority = 1.f;
        VkDeviceQueueCreateInfo queue_info{};
        queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info.queueFamilyIndex = queue_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;

        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;

        const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        VkDeviceCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        info.pNext = &features13;
        info.queueCreateInfoCount = 1;
        info.pQueueCreateInfos = &queue_info;
        info.enabledExtensionCount = 1;
        info.ppEnabledExtensionNames = extensions;
        check(vkCreateDevice(gpu, &info, nullptr, &device), "Creating the Vulkan device");
        vkGetDeviceQueue(device, queue_family, 0, &queue);
    }

    void load_device_functions() {
        create_swapchain = load_device_proc<PFN_vkCreateSwapchainKHR>(device, "vkCreateSwapchainKHR");
        destroy_swapchain = load_device_proc<PFN_vkDestroySwapchainKHR>(device, "vkDestroySwapchainKHR");
        get_swapchain_images = load_device_proc<PFN_vkGetSwapchainImagesKHR>(device, "vkGetSwapchainImagesKHR");
        acquire_next_image = load_device_proc<PFN_vkAcquireNextImageKHR>(device, "vkAcquireNextImageKHR");
        queue_present = load_device_proc<PFN_vkQueuePresentKHR>(device, "vkQueuePresentKHR");
    }

    void choose_format() {
        std::uint32_t count = 0;
        check(surface_formats(gpu, surface, &count, nullptr), "Querying surface formats");
        std::vector<VkSurfaceFormatKHR> formats(count);
        check(surface_formats(gpu, surface, &count, formats.data()), "Reading surface formats");
        if (formats.empty()) {
            fail("The Vulkan surface has no image formats");
        }
        format = formats[0].format;
        for (const VkSurfaceFormatKHR& candidate : formats) {
            const bool color_ok = candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
            const bool rgba = candidate.format == VK_FORMAT_B8G8R8A8_UNORM || candidate.format == VK_FORMAT_R8G8B8A8_UNORM;
            if (color_ok && rgba) {
                format = candidate.format;
                break;
            }
        }
    }

    VkShaderModule make_shader(const std::uint32_t* words, std::size_t bytes) const {
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = bytes;
        info.pCode = words;
        VkShaderModule shader = VK_NULL_HANDLE;
        check(vkCreateShaderModule(device, &info, nullptr, &shader), "Creating a shader module");
        return shader;
    }

    void create_pipeline() {
        VkShaderModule vertex = make_shader(shaders::kQuadVertex, sizeof(shaders::kQuadVertex));
        VkShaderModule fragment = make_shader(shaders::kQuadFragment, sizeof(shaders::kQuadFragment));

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertex;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragment;
        stages[1].pName = "main";

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        VkVertexInputAttributeDescription attributes[2]{};
        attributes[0].location = 0;
        attributes[0].binding = 0;
        attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[0].offset = 0;
        attributes[1].location = 1;
        attributes[1].binding = 0;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, r);

        VkPipelineVertexInputStateCreateInfo vertex_input{};
        vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_input.vertexBindingDescriptionCount = 1;
        vertex_input.pVertexBindingDescriptions = &binding;
        vertex_input.vertexAttributeDescriptionCount = 2;
        vertex_input.pVertexAttributeDescriptions = attributes;

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.f;

        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blend_attachment{};
        blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &blend_attachment;

        const VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamic_states;

        VkPushConstantRange push{};
        push.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push.offset = 0;
        push.size = sizeof(PushConstants);
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(device, &layout_info, nullptr, &pipeline_layout), "Creating the pipeline layout");

        VkPipelineRenderingCreateInfo rendering{};
        rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &format;

        VkGraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.pNext = &rendering;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = stages;
        pipeline_info.pVertexInputState = &vertex_input;
        pipeline_info.pInputAssemblyState = &assembly;
        pipeline_info.pViewportState = &viewport;
        pipeline_info.pRasterizationState = &raster;
        pipeline_info.pMultisampleState = &multisample;
        pipeline_info.pColorBlendState = &blend;
        pipeline_info.pDynamicState = &dynamic;
        pipeline_info.layout = pipeline_layout;
        const VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
        vkDestroyShaderModule(device, vertex, nullptr);
        vkDestroyShaderModule(device, fragment, nullptr);
        check(result, "Creating the graphics pipeline");
    }

    void create_commands() {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = queue_family;
        check(vkCreateCommandPool(device, &pool, nullptr, &command_pool), "Creating the command pool");

        VkCommandBufferAllocateInfo buffers{};
        buffers.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffers.commandPool = command_pool;
        buffers.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buffers.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(device, &buffers, &command_buffer), "Allocating the command buffer");
    }

    void create_sync() {
        destroy_sync();
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        check(vkCreateSemaphore(device, &semaphore, nullptr, &image_available), "Creating the image semaphore");
        check(vkCreateSemaphore(device, &semaphore, nullptr, &render_finished), "Creating the render semaphore");
        if (frame_fence == VK_NULL_HANDLE) {
            VkFenceCreateInfo fence{};
            fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
            fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            check(vkCreateFence(device, &fence, nullptr, &frame_fence), "Creating the frame fence");
        }
    }

    void destroy_sync() {
        if (device == VK_NULL_HANDLE) {
            return;
        }
        if (image_available != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, image_available, nullptr);
            image_available = VK_NULL_HANDLE;
        }
        if (render_finished != VK_NULL_HANDLE) {
            vkDestroySemaphore(device, render_finished, nullptr);
            render_finished = VK_NULL_HANDLE;
        }
    }

    void destroy_swapchain_views() {
        for (VkImageView view : views) {
            vkDestroyImageView(device, view, nullptr);
        }
        views.clear();
        images.clear();
    }

    void recreate_swapchain() {
        vkWaitForFences(device, 1, &frame_fence, VK_TRUE, UINT64_MAX);
        vkDeviceWaitIdle(device);

        VkSurfaceCapabilitiesKHR caps{};
        check(surface_capabilities(gpu, surface, &caps), "Reading surface capabilities");
        VkExtent2D next = caps.currentExtent;
        if (next.width == 0xffffffffu) {
            next.width = static_cast<std::uint32_t>(std::clamp(window_width, 1, 16384));
            next.height = static_cast<std::uint32_t>(std::clamp(window_height, 1, 16384));
            next.width = std::clamp(next.width, caps.minImageExtent.width, caps.maxImageExtent.width);
            next.height = std::clamp(next.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        if (next.width == 0 || next.height == 0) {
            fail("The window surface has a zero-sized drawable");
        }

        std::uint32_t mode_count = 0;
        check(surface_present_modes(gpu, surface, &mode_count, nullptr), "Querying present modes");
        std::vector<VkPresentModeKHR> modes(mode_count);
        check(surface_present_modes(gpu, surface, &mode_count, modes.data()), "Reading present modes");
        bool fifo = false;
        for (VkPresentModeKHR mode : modes) {
            fifo = fifo || mode == VK_PRESENT_MODE_FIFO_KHR;
        }
        if (!fifo) {
            fail("The Vulkan surface does not support FIFO present");
        }

        std::uint32_t image_count = caps.minImageCount + 1;
        if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
            image_count = caps.maxImageCount;
        }

        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if ((caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) == 0) {
            alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }

        VkSwapchainCreateInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        info.surface = surface;
        info.minImageCount = image_count;
        info.imageFormat = format;
        info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        info.imageExtent = next;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = alpha;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = swapchain;

        VkSwapchainKHR fresh = VK_NULL_HANDLE;
        check(create_swapchain(device, &info, nullptr, &fresh), "Creating the swapchain");
        if (swapchain != VK_NULL_HANDLE) {
            destroy_swapchain_views();
            destroy_swapchain(device, swapchain, nullptr);
        }
        swapchain = fresh;
        extent = next;
        dirty = false;

        std::uint32_t acquired = 0;
        check(get_swapchain_images(device, swapchain, &acquired, nullptr), "Counting swapchain images");
        images.resize(acquired);
        check(get_swapchain_images(device, swapchain, &acquired, images.data()), "Reading swapchain images");
        views.resize(acquired);
        for (std::uint32_t index = 0; index < acquired; ++index) {
            VkImageViewCreateInfo view{};
            view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            view.image = images[index];
            view.viewType = VK_IMAGE_VIEW_TYPE_2D;
            view.format = format;
            view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            view.subresourceRange.levelCount = 1;
            view.subresourceRange.layerCount = 1;
            check(vkCreateImageView(device, &view, nullptr, &views[index]), "Creating a swapchain image view");
        }
        create_sync();
    }

    void destroy_vertex_buffer() {
        if (vertex_mapped != nullptr && device != VK_NULL_HANDLE) {
            vkUnmapMemory(device, vertex_memory);
            vertex_mapped = nullptr;
        }
        if (vertex_buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device, vertex_buffer, nullptr);
            vertex_buffer = VK_NULL_HANDLE;
        }
        if (vertex_memory != VK_NULL_HANDLE) {
            vkFreeMemory(device, vertex_memory, nullptr);
            vertex_memory = VK_NULL_HANDLE;
        }
        vertex_capacity = 0;
    }

    void ensure_vertex_capacity(std::size_t vertex_count) {
        const VkDeviceSize bytes = static_cast<VkDeviceSize>(vertex_count * sizeof(Vertex));
        if (bytes <= vertex_capacity) {
            return;
        }
        vkDeviceWaitIdle(device);
        destroy_vertex_buffer();
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = std::max(bytes, static_cast<VkDeviceSize>(256u * 1024u));
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        check(vkCreateBuffer(device, &info, nullptr, &vertex_buffer), "Creating the vertex buffer");

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, vertex_buffer, &requirements);
        VkMemoryAllocateInfo memory{};
        memory.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memory.allocationSize = requirements.size;
        memory.memoryTypeIndex = find_memory_type(
            gpu, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        check(vkAllocateMemory(device, &memory, nullptr, &vertex_memory), "Allocating vertex memory");
        check(vkBindBufferMemory(device, vertex_buffer, vertex_memory, 0), "Binding vertex memory");
        check(vkMapMemory(device, vertex_memory, 0, info.size, 0, &vertex_mapped), "Mapping vertex memory");
        vertex_capacity = info.size;
    }

    ICanvas& begin_frame(int width, int height) {
        if (width > 0 && height > 0 && (dirty || width != window_width || height != window_height)) {
            window_width = width;
            window_height = height;
            recreate_swapchain();
        }
        if (width <= 0 || height <= 0 || extent.width == 0 || extent.height == 0) {
            canvas.begin(0, 0);
        } else {
            canvas.begin(static_cast<int>(extent.width), static_cast<int>(extent.height));
        }
        return canvas;
    }

    bool present() {
        if (canvas.width_ <= 0 || canvas.height_ <= 0 || swapchain == VK_NULL_HANDLE) {
            return false;
        }
        vkWaitForFences(device, 1, &frame_fence, VK_TRUE, UINT64_MAX);

        std::uint32_t image_index = 0;
        const VkResult acquired = acquire_next_image(device, swapchain, UINT64_MAX, image_available, VK_NULL_HANDLE, &image_index);
        if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
            dirty = true;
            return false;
        }
        if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
            check(acquired, "Acquiring a swapchain image");
        }

        std::vector<Vertex> vertices;
        vertices.reserve(canvas.commands_.size() * 6);
        for (const RectCanvas::Command& command : canvas.commands_) {
            const float x = command.rect.x;
            const float y = command.rect.y;
            const float w = command.rect.w;
            const float h = command.rect.h;
            const float r = static_cast<float>(command.color.r) / 255.f;
            const float g = static_cast<float>(command.color.g) / 255.f;
            const float b = static_cast<float>(command.color.b) / 255.f;
            const Vertex quad[] = {
                {x, y, r, g, b},
                {x + w, y, r, g, b},
                {x + w, y + h, r, g, b},
                {x, y, r, g, b},
                {x + w, y + h, r, g, b},
                {x, y + h, r, g, b},
            };
            vertices.insert(vertices.end(), std::begin(quad), std::end(quad));
        }
        if (!vertices.empty()) {
            ensure_vertex_capacity(vertices.size());
            std::memcpy(vertex_mapped, vertices.data(), vertices.size() * sizeof(Vertex));
        }

        check(vkResetCommandBuffer(command_buffer, 0), "Resetting the command buffer");
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(command_buffer, &begin), "Beginning the command buffer");

        VkImageMemoryBarrier to_color{};
        to_color.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        to_color.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        to_color.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        to_color.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_color.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        to_color.image = images[image_index];
        to_color.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        to_color.subresourceRange.levelCount = 1;
        to_color.subresourceRange.layerCount = 1;
        to_color.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vkCmdPipelineBarrier(
            command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_color);

        VkRenderingAttachmentInfo color{};
        color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color.imageView = views[image_index];
        color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.clearValue.color.float32[0] = static_cast<float>(canvas.clear_.r) / 255.f;
        color.clearValue.color.float32[1] = static_cast<float>(canvas.clear_.g) / 255.f;
        color.clearValue.color.float32[2] = static_cast<float>(canvas.clear_.b) / 255.f;
        color.clearValue.color.float32[3] = 1.f;

        VkRenderingInfo rendering{};
        rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering.renderArea.extent = extent;
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &color;
        vkCmdBeginRendering(command_buffer, &rendering);
        if (!vertices.empty()) {
            vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &offset);
            VkViewport viewport{};
            viewport.width = static_cast<float>(extent.width);
            viewport.height = static_cast<float>(extent.height);
            viewport.minDepth = 0.f;
            viewport.maxDepth = 1.f;
            vkCmdSetViewport(command_buffer, 0, 1, &viewport);
            VkRect2D scissor{};
            scissor.extent = extent;
            vkCmdSetScissor(command_buffer, 0, 1, &scissor);
            const PushConstants push{static_cast<float>(extent.width), static_cast<float>(extent.height)};
            vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
            vkCmdDraw(command_buffer, static_cast<std::uint32_t>(vertices.size()), 1, 0, 0);
        }
        vkCmdEndRendering(command_buffer);

        VkImageMemoryBarrier to_present = to_color;
        to_present.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        to_present.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        to_present.dstAccessMask = 0;
        vkCmdPipelineBarrier(
            command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &to_present);
        check(vkEndCommandBuffer(command_buffer), "Ending the command buffer");

        check(vkResetFences(device, 1, &frame_fence), "Resetting the frame fence");
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &image_available;
        submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command_buffer;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &render_finished;
        check(vkQueueSubmit(queue, 1, &submit, frame_fence), "Submitting the frame");

        VkPresentInfoKHR present_info{};
        present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = &render_finished;
        present_info.swapchainCount = 1;
        present_info.pSwapchains = &swapchain;
        present_info.pImageIndices = &image_index;
        const VkResult presented = queue_present(queue, &present_info);
        if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR || acquired == VK_SUBOPTIMAL_KHR) {
            dirty = true;
        } else {
            check(presented, "Presenting the swapchain image");
        }
        return true;
    }
};

Renderer::Renderer(XcbWindow& window) : impl_(std::make_unique<Impl>()) {
    impl_->init(window);
}

Renderer::~Renderer() = default;

ICanvas& Renderer::begin_frame(int window_width, int window_height) {
    return impl_->begin_frame(window_width, window_height);
}

bool Renderer::present() { return impl_->present(); }

}  // namespace lla
