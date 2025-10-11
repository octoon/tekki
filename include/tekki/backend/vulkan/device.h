#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <string>
#include <cstring>
#include <stdexcept>
#include <functional>
#include <array>
#include <optional>
#include <glm/glm.hpp>
#include "vulkan/vulkan.h"
#include <tekki/gpu_allocator/vulkan/allocator.h>
#include <tekki/gpu_profiler/gpu_profiler.h>
#include "tekki/core/result.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/vulkan/physical_device.h"
#include "tekki/backend/vulkan/profiler.h"
#include "tekki/backend/vulkan/instance.h"
#include "tekki/backend/vulkan/error.h"
#include "tekki/backend/vulkan/debug_utils.h"

namespace tekki::backend {
    class DynamicConstants;
}

namespace tekki::backend::vulkan {

class CrashMarkerNames;
class BackendError;
class RayTracingAcceleration;
class RayTracingAccelerationScratchBuffer;
struct RayTracingBottomAccelerationDesc;
struct RayTracingTopAccelerationDesc;
struct RayTracingInstanceDesc;

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
                tekki::Allocator* globalAllocator,
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
    std::shared_ptr<tekki::Allocator> globalAllocator_;
    std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash> immutableSamplers_;
    std::mutex setupCbMutex_;
    std::optional<CommandBuffer> setupCb_;
    Buffer crashTrackingBuffer_;
    std::mutex crashMarkerNamesMutex_;
    CrashMarkerNames crashMarkerNames_;
    VkAccelerationStructureKHR accelerationStructureExt_;
    void* rayTracingPipelineExt_;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties_;
    std::array<std::mutex, 2> frameMutexes_;
    std::array<std::shared_ptr<DeviceFrame>, 2> frames_;
    bool rayTracingEnabled_;
    std::unique_ptr<class DebugUtils> debugUtils_;

    static std::unordered_map<SamplerDesc, VkSampler, SamplerDescHash> CreateSamplers(VkDevice device);
    static Buffer CreateBufferImpl(VkDevice device, tekki::Allocator* allocator,
                                  const BufferDesc& desc, const std::string& name);
    void ReportError(const std::exception& error) const;

public:
    // Factory method - use this to create Device instances
    static std::shared_ptr<Device> Create(const std::shared_ptr<PhysicalDevice>& pdevice);

    // Default constructor - DO NOT CALL DIRECTLY, use Create() instead
    Device() = default;
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
    void ImmediateDestroyBuffer(Buffer buffer);

    // Error handling
    void RecordCrashMarker(const CommandBuffer& cb, const std::string& name);
    void ReportError(const BackendError& err);

    // Ray tracing
    std::shared_ptr<RayTracingAccelerationScratchBuffer> CreateRayTracingAccelerationScratchBuffer();
    std::shared_ptr<RayTracingAcceleration> CreateRayTracingBottomAcceleration(const RayTracingBottomAccelerationDesc& desc);
    std::shared_ptr<RayTracingAcceleration> CreateRayTracingTopAcceleration(const RayTracingTopAccelerationDesc& desc, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer);
    VkDeviceAddress FillRayTracingInstanceBuffer(backend::DynamicConstants& dynamicConstants, const std::vector<RayTracingInstanceDesc>& instances);
    void RebuildRayTracingTopAcceleration(VkCommandBuffer commandBuffer, VkDeviceAddress instanceBufferAddress, size_t instanceCount, const std::shared_ptr<RayTracingAcceleration>& tlas, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer);
    std::shared_ptr<class RayTracingShaderTable> CreateRayTracingShaderTable(const struct RayTracingShaderTableDesc& desc, VkPipeline pipeline);

private:
    std::shared_ptr<RayTracingAcceleration> CreateRayTracingAcceleration(VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildGeometryInfoKHR geometryInfo, const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& buildRangeInfos, const std::vector<uint32_t>& maxPrimitiveCounts, size_t preallocateBytes, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer);
    void RebuildRayTracingAcceleration(VkCommandBuffer commandBuffer, VkAccelerationStructureBuildGeometryInfoKHR geometryInfo, const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& buildRangeInfos, const std::vector<uint32_t>& maxPrimitiveCounts, const std::shared_ptr<RayTracingAcceleration>& acceleration, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer);

public:
    VkDevice GetRaw() const { return raw_; }
    VkDevice GetRawDevice() const { return raw_; }  // Alias for compatibility
    const Queue& GetUniversalQueue() const { return universalQueue_; }

    // Friend the non-member function so it can access private members
    template<typename T> friend class PipelineShader;
    friend std::shared_ptr<class RayTracingPipeline> CreateRayTracingPipeline(const std::shared_ptr<Device>&, const std::vector<PipelineShader<std::vector<uint32_t>>>&, const struct RayTracingPipelineDesc&);
};

inline void DescriptorPoolDeferredRelease::EnqueueRelease(PendingResourceReleases& pending) const {
    pending.DescriptorPools.push_back(pool_);
}

} // namespace tekki::backend::vulkan