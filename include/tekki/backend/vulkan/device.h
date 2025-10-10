#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <string>
#include <cstring>
#include <stdexcept>
#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include <tekki/gpu_allocator/vulkan/allocator.h>
#include <tekki/gpu_profiler/gpu_profiler.h>
#include "tekki/core/result.h"
#include "vulkan/buffer.h"
#include "vulkan/physical_device.h"
#include "vulkan/profiler.h"
#include "vulkan/instance.h"
#include "vulkan/error.h"

namespace tekki::backend::vulkan {

constexpr uint32_t ReservedDescriptorCount = 32;

struct Queue {
    VkQueue Raw;
    QueueFamily Family;
};

class DeferredRelease {
public:
    virtual ~DeferredRelease() = default;
    virtual void EnqueueRelease(class PendingResourceReleases& pending) const = 0;
};

class DescriptorPoolDeferredRelease : public DeferredRelease {
private:
    VkDescriptorPool pool_;
    
public:
    explicit DescriptorPoolDeferredRelease(VkDescriptorPool pool) : pool_(pool) {}
    
    void EnqueueRelease(PendingResourceReleases& pending) const override;
};

class PendingResourceReleases {
public:
    std::vector<VkDescriptorPool> DescriptorPools;
    
    PendingResourceReleases() = default;
    
    void ReleaseAll(VkDevice device);
};

class CommandBuffer {
public:
    VkCommandBuffer Raw;
    VkFence SubmitDoneFence;
    
    CommandBuffer(VkDevice device, const QueueFamily& queueFamily);
    ~CommandBuffer() = default;
    
    CommandBuffer(const CommandBuffer&) = delete;
    CommandBuffer& operator=(const CommandBuffer&) = delete;
    CommandBuffer(CommandBuffer&&) = delete;
    CommandBuffer& operator=(CommandBuffer&&) = delete;
};

class DeviceFrame {
public:
    std::optional<VkSemaphore> SwapchainAcquiredSemaphore;
    std::optional<VkSemaphore> RenderingCompleteSemaphore;
    CommandBuffer MainCommandBuffer;
    CommandBuffer PresentationCommandBuffer;
    std::mutex PendingResourceReleasesMutex;
    PendingResourceReleases PendingReleases;
    VkProfilerData ProfilerData;
    
    DeviceFrame(const PhysicalDevice* pdevice, VkDevice device,
                gpu_allocator::Allocator* globalAllocator,
                const QueueFamily& queueFamily);
    ~DeviceFrame() = default;
    
    DeviceFrame(const DeviceFrame&) = delete;
    DeviceFrame& operator=(const DeviceFrame&) = delete;
    DeviceFrame(DeviceFrame&&) = delete;
    DeviceFrame& operator=(DeviceFrame&&) = delete;
};

struct SamplerDesc {
    VkFilter TexelFilter;
    VkSamplerMipmapMode MipmapMode;
    VkSamplerAddressMode AddressModes;
    
    bool operator==(const SamplerDesc& other) const {
        return TexelFilter == other.TexelFilter &&
               MipmapMode == other.MipmapMode &&
               AddressModes == other.AddressModes;
    }
};

struct SamplerDescHash {
    std::size_t operator()(const SamplerDesc& desc) const {
        return std::hash<uint32_t>{}(static_cast<uint32_t>(desc.TexelFilter)) ^
               (std::hash<uint32_t>{}(static_cast<uint32_t>(desc.MipmapMode)) << 1) ^
               (std::hash<uint32_t>{}(static_cast<uint32_t>(desc.AddressModes)) << 2);
    }
};

class Device {
private:
    VkDevice raw_;
    std::shared_ptr<PhysicalDevice> pdevice_;
    std::shared_ptr<Instance> instance_;
    Queue universalQueue_;
    std::shared_ptr<std::mutex> globalAllocatorMutex_;
    std::shared_ptr<gpu_allocator::Allocator> globalAllocator_;
    std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash> immutableSamplers_;
    std::mutex setupCbMutex_;
    CommandBuffer setupCb_;
    Buffer crashTrackingBuffer_;
    std::mutex crashMarkerNamesMutex_;
    CrashMarkerNames crashMarkerNames_;
    VkAccelerationStructureKHR accelerationStructureExt_;
    VkRayTracingPipelineKHR rayTracingPipelineExt_;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties_;
    std::array<std::mutex, 2> frameMutexes_;
    std::array<std::shared_ptr<DeviceFrame>, 2> frames_;
    bool rayTracingEnabled_;
    
    static std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash> CreateSamplers(VkDevice device);
    static Buffer CreateBufferImpl(VkDevice device, gpu_allocator::Allocator* allocator,
                                  const BufferDesc& desc, const std::string& name);
    void ReportError(const std::exception& error) const;

public:
    static std::shared_ptr<Device> Create(const std::shared_ptr<PhysicalDevice>& pdevice);
    ~Device();
    
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;
    
    VkSampler GetSampler(const SamplerDesc& desc) const;
    std::shared_ptr<DeviceFrame> BeginFrame();
    void DeferRelease(const DeferredRelease& resource);
    void WithSetupCb(const std::function<void(VkCommandBuffer)>& callback);
    void FinishFrame(const std::shared_ptr<DeviceFrame>& frame);
    const PhysicalDevice* PhysicalDevice() const;
    const class DebugUtils* DebugUtils() const;
    uint32_t MaxBindlessDescriptorCount() const;
    bool RayTracingEnabled() const;

    // Buffer management
    Buffer CreateBuffer(BufferDesc desc, const std::string& name, const std::vector<uint8_t>& initialData = {});
    void ImmediateDestroyBuffer(const Buffer& buffer);

    VkDevice GetRaw() const { return raw_; }
    const Queue& GetUniversalQueue() const { return universalQueue_; }
};

inline void DescriptorPoolDeferredRelease::EnqueueRelease(PendingResourceReleases& pending) const {
    pending.DescriptorPools.push_back(pool_);
}

} // namespace tekki::backend::vulkan