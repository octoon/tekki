#include "tekki/backend/vulkan/device.h"
#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <unordered_set>
#include <format>
#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include "tekki/gpu_allocator/vulkan/allocator.h"
#include "tekki/gpu_profiler/gpu_profiler.h"
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/physical_device.h"
#include "tekki/backend/vulkan/profiler.h"
#include "tekki/backend/vulkan/instance.h"
#include "tekki/backend/vulkan/error.h"
#include "tekki/backend/vulkan/debug_utils.h"

namespace tekki::backend::vulkan {

CommandBuffer::CommandBuffer(VkDevice device, const QueueFamily& queueFamily) {
    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = queueFamily.index;

    VkCommandPool pool;
    if (vkCreateCommandPool(device, &poolCreateInfo, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool");
    }

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = pool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocateInfo, &Raw) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateFence(device, &fenceCreateInfo, nullptr, &SubmitDoneFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create fence");
    }
}

DeviceFrame::DeviceFrame(const PhysicalDevice* pdevice, VkDevice device,
                        tekki::Allocator* globalAllocator,
                        const QueueFamily& queueFamily)
    : MainCommandBuffer(device, queueFamily)
    , PresentationCommandBuffer(device, queueFamily)
    , ProfilerData(device, *std::make_unique<ProfilerBackend>(
        device,
        globalAllocator,
        pdevice->properties.limits.timestampPeriod
    ))
    , SwapchainAcquiredSemaphore(std::nullopt)
    , RenderingCompleteSemaphore(std::nullopt) {
    // TODO: Initialize linear allocator pool if needed
}

void PendingResourceReleases::ReleaseAll(VkDevice device) {
    for (auto pool : DescriptorPools) {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    DescriptorPools.clear();
}

std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash> Device::CreateSamplers(VkDevice device) {
    std::vector<VkFilter> texelFilters = {VK_FILTER_NEAREST, VK_FILTER_LINEAR};
    std::vector<VkSamplerMipmapMode> mipmapModes = {VK_SAMPLER_MIPMAP_MODE_NEAREST, VK_SAMPLER_MIPMAP_MODE_LINEAR};
    std::vector<VkSamplerAddressMode> addressModes = {VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE};

    std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash> result;

    for (auto texelFilter : texelFilters) {
        for (auto mipmapMode : mipmapModes) {
            for (auto addressMode : addressModes) {
                bool anisotropyEnable = (texelFilter == VK_FILTER_LINEAR);

                VkSamplerCreateInfo samplerInfo{};
                samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                samplerInfo.magFilter = texelFilter;
                samplerInfo.minFilter = texelFilter;
                samplerInfo.mipmapMode = mipmapMode;
                samplerInfo.addressModeU = addressMode;
                samplerInfo.addressModeV = addressMode;
                samplerInfo.addressModeW = addressMode;
                samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
                samplerInfo.maxAnisotropy = 16.0f;
                samplerInfo.anisotropyEnable = anisotropyEnable;

                VkSampler sampler;
                if (vkCreateSampler(device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to create sampler");
                }

                SamplerDesc desc;
                desc.TexelFilter = texelFilter;
                desc.MipmapMode = mipmapMode;
                desc.AddressModes = addressMode;

                result[desc] = sampler;
            }
        }
    }

    return result;
}

Buffer Device::CreateBufferImpl(VkDevice device, tekki::Allocator* allocator,
                               const BufferDesc& desc, const std::string& name) {
    (void)name; // Mark as unused for now
    // Create buffer using the allocator
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.Size;
    bufferInfo.usage = desc.Usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer buffer;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer");
    }

    // Allocate memory for the buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    tekki::AllocationCreateDesc allocDesc{};
    allocDesc.requirements = memRequirements;
    allocDesc.location = desc.MemoryLocation;
    allocDesc.linear = false;
    allocDesc.allocation_scheme = tekki::AllocationScheme::GpuAllocatorManaged;

    auto allocation = allocator->Allocate(allocDesc);
    if (allocation.IsNull()) {
        vkDestroyBuffer(device, buffer, nullptr);
        throw std::runtime_error("Failed to allocate buffer memory");
    }

    // Bind memory to buffer
    if (vkBindBufferMemory(device, buffer, allocation.Memory(), allocation.Offset()) != VK_SUCCESS) {
        vkDestroyBuffer(device, buffer, nullptr);
        allocator->Free(std::move(allocation));
        throw std::runtime_error("Failed to bind buffer memory");
    }

    // Construct and return Buffer struct
    Buffer result;
    result.Raw = buffer;
    result.Desc = desc;
    result.Allocation = std::move(allocation);
    return result;
}

void Device::ReportError(const std::exception& error) const {
    // Log the error - implementation depends on logging system
    // For now, we'll just rethrow
    throw error;
}

std::shared_ptr<Device> Device::Create(const std::shared_ptr<tekki::backend::vulkan::PhysicalDevice>& pdevice) {
    // Query supported device extensions
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(pdevice->raw, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> extensionProperties(extensionCount);
    vkEnumerateDeviceExtensionProperties(pdevice->raw, nullptr, &extensionCount, extensionProperties.data());

    std::unordered_set<std::string> supportedExtensions;
    for (const auto& ext : extensionProperties) {
        supportedExtensions.insert(ext.extensionName);
    }

    std::vector<const char*> deviceExtensionNames = {
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_2_EXTENSION_NAME,
        VK_KHR_MAINTENANCE_3_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_KHR_IMAGELESS_FRAMEBUFFER_EXTENSION_NAME,
        VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
        VK_KHR_DESCRIPTOR_UPDATE_TEMPLATE_EXTENSION_NAME,
        VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,  // Add debug utils extension
    };

    // Add DLSS extensions if enabled
    #ifdef TEKKI_DLSS
    deviceExtensionNames.push_back("VK_NVX_binary_import");
    deviceExtensionNames.push_back("VK_KHR_push_descriptor");
    deviceExtensionNames.push_back(VK_NVX_IMAGE_VIEW_HANDLE_EXTENSION_NAME);
    #endif

    std::vector<const char*> rayTracingExtensions = {
        VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    };

    bool rayTracingEnabled = true;
    for (const auto& ext : rayTracingExtensions) {
        if (supportedExtensions.find(ext) == supportedExtensions.end()) {
            rayTracingEnabled = false;
            break;
        }
    }

    if (rayTracingEnabled) {
        deviceExtensionNames.insert(deviceExtensionNames.end(), 
                                  rayTracingExtensions.begin(), rayTracingExtensions.end());
    }

    if (pdevice->presentationRequested) {
        deviceExtensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    // Verify all extensions are supported
    for (const auto& ext : deviceExtensionNames) {
        if (supportedExtensions.find(ext) == supportedExtensions.end()) {
            throw std::runtime_error(std::string("Device extension not supported: ") + ext);
        }
    }

    // Find universal queue
    const QueueFamily* universalQueueFamily = nullptr;
    for (const auto& qf : pdevice->queueFamilies) {
        if (qf.properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            universalQueueFamily = &qf;
            break;
        }
    }

    if (!universalQueueFamily) {
        throw std::runtime_error("No suitable render queue found");
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = universalQueueFamily->index;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Feature structures
    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT scalarBlock{};
    scalarBlock.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT;
    
    VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing{};
    descriptorIndexing.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT;
    
    VkPhysicalDeviceImagelessFramebufferFeaturesKHR imagelessFramebuffer{};
    imagelessFramebuffer.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES_KHR;
    
    VkPhysicalDeviceShaderFloat16Int8Features shaderFloat16Int8{};
    shaderFloat16Int8.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_FLOAT16_INT8_FEATURES;
    
    VkPhysicalDeviceVulkanMemoryModelFeaturesKHR vulkanMemoryModel{};
    vulkanMemoryModel.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES_KHR;
    
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddress{};
    bufferDeviceAddress.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

    // Chain features
    void* pNext = &scalarBlock;
    scalarBlock.pNext = &descriptorIndexing;
    descriptorIndexing.pNext = &imagelessFramebuffer;
    imagelessFramebuffer.pNext = &shaderFloat16Int8;
    shaderFloat16Int8.pNext = &vulkanMemoryModel;
    vulkanMemoryModel.pNext = &bufferDeviceAddress;

    if (rayTracingEnabled) {
        bufferDeviceAddress.pNext = &accelerationStructureFeatures;
        accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;
    }

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = pNext;

    vkGetPhysicalDeviceFeatures2(pdevice->raw, &features2);

    // Validate required features
    if (!scalarBlock.scalarBlockLayout) {
        throw std::runtime_error("Scalar block layout not supported");
    }

    if (!descriptorIndexing.shaderUniformTexelBufferArrayDynamicIndexing ||
        !descriptorIndexing.shaderStorageTexelBufferArrayDynamicIndexing ||
        !descriptorIndexing.shaderSampledImageArrayNonUniformIndexing ||
        !descriptorIndexing.shaderStorageImageArrayNonUniformIndexing ||
        !descriptorIndexing.shaderUniformTexelBufferArrayNonUniformIndexing ||
        !descriptorIndexing.shaderStorageTexelBufferArrayNonUniformIndexing ||
        !descriptorIndexing.descriptorBindingSampledImageUpdateAfterBind ||
        !descriptorIndexing.descriptorBindingUpdateUnusedWhilePending ||
        !descriptorIndexing.descriptorBindingPartiallyBound ||
        !descriptorIndexing.descriptorBindingVariableDescriptorCount ||
        !descriptorIndexing.runtimeDescriptorArray) {
        throw std::runtime_error("Required descriptor indexing features not supported");
    }

    if (!imagelessFramebuffer.imagelessFramebuffer) {
        throw std::runtime_error("Imageless framebuffer not supported");
    }

    if (!shaderFloat16Int8.shaderInt8) {
        throw std::runtime_error("Shader int8 not supported");
    }

    if (rayTracingEnabled) {
        if (!descriptorIndexing.shaderUniformBufferArrayNonUniformIndexing ||
            !descriptorIndexing.shaderStorageBufferArrayNonUniformIndexing) {
            throw std::runtime_error("Required ray tracing descriptor indexing features not supported");
        }

        if (!vulkanMemoryModel.vulkanMemoryModel) {
            throw std::runtime_error("Vulkan memory model not supported");
        }

        if (!accelerationStructureFeatures.accelerationStructure ||
            !accelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind) {
            throw std::runtime_error("Required acceleration structure features not supported");
        }

        if (!rayTracingPipelineFeatures.rayTracingPipeline ||
            !rayTracingPipelineFeatures.rayTracingPipelineTraceRaysIndirect) {
            throw std::runtime_error("Required ray tracing pipeline features not supported");
        }

        if (!bufferDeviceAddress.bufferDeviceAddress) {
            throw std::runtime_error("Buffer device address not supported");
        }
    }

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensionNames.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames.data();
    deviceCreateInfo.pNext = &features2;

    VkDevice rawDevice;
    if (vkCreateDevice(pdevice->raw, &deviceCreateInfo, nullptr, &rawDevice) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan device");
    }

    // Create global allocator
    tekki::AllocatorCreateDesc allocatorDesc{};
    allocatorDesc.instance = pdevice->instance->GetRaw();
    allocatorDesc.device = rawDevice;
    allocatorDesc.physical_device = pdevice->raw;
    allocatorDesc.debug_settings.log_leaks_on_shutdown = false;
    allocatorDesc.debug_settings.log_memory_information = true;
    allocatorDesc.debug_settings.log_allocations = true;
    allocatorDesc.buffer_device_address = true;
    // TODO: Set allocation_sizes if needed

    auto globalAllocator = std::make_shared<tekki::Allocator>(allocatorDesc);

    // Get universal queue
    VkQueue universalQueueRaw;
    vkGetDeviceQueue(rawDevice, universalQueueFamily->index, 0, &universalQueueRaw);
    
    Queue universalQueue{universalQueueRaw, *universalQueueFamily};

    // Create device frames
    auto frame0 = std::make_shared<DeviceFrame>(pdevice.get(), rawDevice, globalAllocator.get(), universalQueue.Family);
    auto frame1 = std::make_shared<DeviceFrame>(pdevice.get(), rawDevice, globalAllocator.get(), universalQueue.Family);

    auto immutableSamplers = CreateSamplers(rawDevice);
    CommandBuffer setupCb(rawDevice, universalQueue.Family);

    // Load ray tracing extensions if enabled
    VkAccelerationStructureKHR accelerationStructureExt = VK_NULL_HANDLE;
    void* rayTracingPipelineExt = nullptr;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};

    if (rayTracingEnabled) {
        // Note: In C++ we'd typically use a library like Volk or manually load extensions
        // This is a simplified representation
        accelerationStructureExt = VK_NULL_HANDLE; // Would be properly initialized
        rayTracingPipelineExt = nullptr; // Would be properly initialized

        VkPhysicalDeviceProperties2 properties2{};
        properties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
        properties2.pNext = &rayTracingPipelineProperties;
        rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

        vkGetPhysicalDeviceProperties2(pdevice->raw, &properties2);
    }

    // Create crash tracking buffer
    BufferDesc crashBufferDesc;
    crashBufferDesc.Size = 4;
    crashBufferDesc.Usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    crashBufferDesc.MemoryLocation = tekki::MemoryLocation::GpuToCpu;
    
    Buffer crashTrackingBuffer = CreateBufferImpl(rawDevice, globalAllocator.get(), crashBufferDesc, "crash tracking buffer");

    auto device = std::make_shared<Device>();
    device->raw_ = rawDevice;
    device->pdevice_ = pdevice;
    device->instance_ = pdevice->instance;
    device->universalQueue_ = universalQueue;
    device->globalAllocatorMutex_ = std::make_shared<std::mutex>();
    device->globalAllocator_ = globalAllocator;
    device->immutableSamplers_ = immutableSamplers;
    // Note: setupCb_ is already initialized in Device class, reuse the one we created
    // device->setupCb_ is default initialized
    device->crashTrackingBuffer_ = std::move(crashTrackingBuffer);
    device->crashMarkerNames_ = CrashMarkerNames();
    device->accelerationStructureExt_ = accelerationStructureExt;
    device->rayTracingPipelineExt_ = rayTracingPipelineExt;
    device->rayTracingPipelineProperties_ = rayTracingPipelineProperties;
    device->frames_[0] = frame0;
    device->frames_[1] = frame1;
    device->rayTracingEnabled_ = rayTracingEnabled;

    // Create DebugUtils if the extension is supported
    if (supportedExtensions.find(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedExtensions.end()) {
        try {
            device->debugUtils_ = std::make_unique<class DebugUtils>(rawDevice, pdevice->instance->GetRaw());
        } catch ([[maybe_unused]] const std::exception& e) {
            // If debug utils creation fails, just log and continue without it
            // This is not a critical error
        }
    }

    return device;
}

Device::~Device() {
    if (raw_) {
        vkDeviceWaitIdle(raw_);
        
        // Clean up samplers
        for (auto& [desc, sampler] : immutableSamplers_) {
            vkDestroySampler(raw_, sampler, nullptr);
        }
        
        // Clean up device frames
        frames_[0].reset();
        frames_[1].reset();
        
        vkDestroyDevice(raw_, nullptr);
    }
}

VkSampler Device::GetSampler(const SamplerDesc& desc) const {
    auto it = immutableSamplers_.find(desc);
    if (it == immutableSamplers_.end()) {
        throw std::runtime_error("Sampler not found");
    }
    return it->second;
}

std::shared_ptr<DeviceFrame> Device::BeginFrame() {
    std::lock_guard lock(frameMutexes_[0]);
    
    auto frame = frames_[0];
    if (!frame) {
        throw std::runtime_error("Frame not available");
    }

    // Wait for GPU to complete previous frame
    VkFence fences[] = {
        frame->MainCommandBuffer.SubmitDoneFence,
        frame->PresentationCommandBuffer.SubmitDoneFence
    };
    
    if (vkWaitForFences(raw_, 2, fences, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for fences");
    }

    // Release pending resources
    {
        std::lock_guard resourceLock(frame->PendingResourceReleasesMutex);
        frame->PendingReleases.ReleaseAll(raw_);
    }

    return frame;
}

void Device::DeferRelease(const DeferredRelease& resource) {
    auto frame = frames_[0];
    if (frame) {
        std::lock_guard lock(frame->PendingResourceReleasesMutex);
        resource.EnqueueRelease(frame->PendingReleases);
    }
}

void Device::WithSetupCb(const std::function<void(VkCommandBuffer)>& callback) {
    std::lock_guard lock(setupCbMutex_);

    // Create setup command buffer if it doesn't exist
    if (!setupCb_.has_value()) {
        setupCb_.emplace(raw_, universalQueue_.Family);
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(setupCb_->Raw, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin command buffer");
    }

    callback(setupCb_->Raw);

    if (vkEndCommandBuffer(setupCb_->Raw) != VK_SUCCESS) {
        throw std::runtime_error("Failed to end command buffer");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &setupCb_->Raw;

    if (vkQueueSubmit(universalQueue_.Raw, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit command buffer");
    }

    if (vkDeviceWaitIdle(raw_) != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for device idle");
    }
}

void Device::FinishFrame([[maybe_unused]] const std::shared_ptr<DeviceFrame>& frame) {
    // Release the frame reference - just let the shared_ptr go out of scope
    // frame.reset() is not needed as we're just releasing our reference

    std::lock_guard lock0(frameMutexes_[0]);
    std::lock_guard lock1(frameMutexes_[1]);

    // Swap frames
    std::swap(frames_[0], frames_[1]);
}

const PhysicalDevice* Device::PhysicalDevice() const {
    return pdevice_.get();
}

const class DebugUtils* Device::DebugUtils() const {
    return debugUtils_.get();
}

uint32_t Device::MaxBindlessDescriptorCount() const {
    return std::min(512u * 1024u, 
                   pdevice_->properties.limits.maxPerStageDescriptorSampledImages - ReservedDescriptorCount);
}

bool Device::RayTracingEnabled() const {
    return rayTracingEnabled_;
}

void Device::RecordCrashMarker([[maybe_unused]] const CommandBuffer& cb, const std::string& name) {
    std::lock_guard<std::mutex> lock(crashMarkerNamesMutex_);
    [[maybe_unused]] uint32_t idx = crashMarkerNames_.InsertName(name);

    try {
        // TODO: Implement crash marker recording
        // Raw.cmd_fill_buffer(cb.Raw, crashTrackingBuffer_.GetRaw(), 0, 4, idx);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::format("Failed to record crash marker: {}", e.what()));
    }
}

void Device::ReportError(const BackendError& err) {
    if (err.GetType() == BackendErrorType::Vulkan &&
        err.GetVulkanResult() == VK_ERROR_DEVICE_LOST) {

        // Something went very wrong. Find the last marker which was successfully written
        // to the crash tracking buffer, and report its corresponding name.
        auto mappedSlice = crashTrackingBuffer_.Allocation.MappedSlice();
        if (!mappedSlice) {
            throw std::runtime_error("Failed to get mapped slice from crash tracking buffer");
        }
        uint32_t lastMarker = *reinterpret_cast<const uint32_t*>(mappedSlice);

        std::lock_guard<std::mutex> lock(crashMarkerNamesMutex_);
        std::string lastMarkerName = crashMarkerNames_.GetName(lastMarker);

        std::string msg;
        if (!lastMarkerName.empty()) {
            msg = std::format(
                "The GPU device has been lost. This is usually due to an infinite loop in a shader.\n"
                "The last crash marker was: {} => {}. The problem most likely exists directly after.",
                lastMarker, lastMarkerName
            );
        } else {
            msg = std::format(
                "The GPU device has been lost. This is usually due to an infinite loop in a shader.\n"
                "The last crash marker was: {}. The problem most likely exists directly after.",
                lastMarker
            );
        }

        throw std::runtime_error(msg);
    }

    throw err;
}

} // namespace tekki::backend::vulkan