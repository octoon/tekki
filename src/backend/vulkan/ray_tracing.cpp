#include "tekki/backend/vulkan/ray_tracing.h"
#include <memory>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"
#include "tekki/backend/dynamic_constants.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vulkan/buffer.h"
#include <rspirv-reflect/reflection.h>
#include <stdexcept>
#include <mutex>

namespace tekki::backend::vulkan {

std::shared_ptr<RayTracingAccelerationScratchBuffer> Device::CreateRayTracingAccelerationScratchBuffer() {
    try {
        BufferDesc bufferDesc = BufferDesc::NewGpuOnly(
            RT_TLAS_SCRATCH_BUFFER_SIZE,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        );

        auto buffer = CreateBuffer(bufferDesc, "Acceleration structure scratch buffer");

        auto scratchBuffer = std::make_shared<RayTracingAccelerationScratchBuffer>();
        scratchBuffer->BufferMutex = std::make_shared<std::mutex>();
        scratchBuffer->Buffer = std::move(buffer);

        return scratchBuffer;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create ray tracing acceleration scratch buffer: ") + e.what());
    }
}

std::shared_ptr<RayTracingAcceleration> Device::CreateRayTracingBottomAcceleration(const RayTracingBottomAccelerationDesc& desc) {
    try {
        //log::trace!("Creating ray tracing bottom acceleration: {:?}", desc);

        std::vector<VkAccelerationStructureGeometryKHR> geometries;
        geometries.reserve(desc.Geometries.size());

        for (const auto& geometryDesc : desc.Geometries) {
            const auto& part = geometryDesc.Parts[0];

            VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{};
            trianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            trianglesData.vertexData.deviceAddress = geometryDesc.VertexBuffer;
            trianglesData.vertexStride = static_cast<VkDeviceSize>(geometryDesc.VertexStride);
            trianglesData.maxVertex = part.MaxVertex;
            trianglesData.vertexFormat = geometryDesc.VertexFormat;
            trianglesData.indexData.deviceAddress = geometryDesc.IndexBuffer;
            trianglesData.indexType = VK_INDEX_TYPE_UINT32; // TODO

            VkAccelerationStructureGeometryKHR geometry{};
            geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geometry.geometry.triangles = trianglesData;
            geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

            geometries.push_back(geometry);
        }

        std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
        buildRangeInfos.reserve(desc.Geometries.size());

        for (const auto& geometryDesc : desc.Geometries) {
            VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
            rangeInfo.primitiveCount = static_cast<uint32_t>(geometryDesc.Parts[0].IndexCount / 3);
            buildRangeInfos.push_back(rangeInfo);
        }

        VkAccelerationStructureBuildGeometryInfoKHR geometryInfo{};
        geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        geometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        geometryInfo.geometryCount = static_cast<uint32_t>(geometries.size());
        geometryInfo.pGeometries = geometries.data();
        geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        std::vector<uint32_t> maxPrimitiveCounts;
        maxPrimitiveCounts.reserve(desc.Geometries.size());
        for (const auto& geometryDesc : desc.Geometries) {
            maxPrimitiveCounts.push_back(static_cast<uint32_t>(geometryDesc.Parts[0].IndexCount / 3));
        }

        size_t preallocateBytes = 0;
        return CreateRayTracingAcceleration(
            VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            geometryInfo,
            buildRangeInfos,
            maxPrimitiveCounts,
            preallocateBytes,
            nullptr
        );
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create ray tracing bottom acceleration: ") + e.what());
    }
}

std::shared_ptr<RayTracingAcceleration> Device::CreateRayTracingTopAcceleration(const RayTracingTopAccelerationDesc& desc, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer) {
    try {
        //log::trace!("Creating ray tracing top acceleration: {:?}", desc);

        // Create instance buffer
        std::vector<GeometryInstance> instances;
        instances.reserve(desc.Instances.size());

        for (const auto& instanceDesc : desc.Instances) {
            VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
            addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
            addressInfo.accelerationStructure = instanceDesc.Blas->Raw;

            VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(raw_, &addressInfo);

            const auto& transform = instanceDesc.Transformation;
            float transformArray[12] = {
                transform[0][0], transform[1][0], transform[2][0], transform[3][0],
                transform[0][1], transform[1][1], transform[2][1], transform[3][1],
                transform[0][2], transform[1][2], transform[2][2], transform[3][2]
            };

            instances.emplace_back(
                transformArray,
                instanceDesc.MeshIndex, // instance id
                0xff,
                0,
                VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR,
                blasAddress
            );
        }

        size_t instanceBufferSize = sizeof(GeometryInstance) * std::max(instances.size(), size_t{1});
        BufferDesc instanceBufferDesc = BufferDesc::NewGpuOnly(
            instanceBufferSize,
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        );

        Buffer instanceBuffer;
        if (!instances.empty()) {
            std::vector<uint8_t> instanceData(instances.size() * sizeof(GeometryInstance));
            std::memcpy(instanceData.data(), instances.data(), instanceData.size());
            instanceBuffer = CreateBuffer(instanceBufferDesc, "TLAS instance buffer", instanceData);
        } else {
            instanceBuffer = CreateBuffer(instanceBufferDesc, "TLAS instance buffer");
        }

        VkDeviceAddress instanceBufferAddress = instanceBuffer.DeviceAddress(raw_);

        VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
        instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        instancesData.data.deviceAddress = instanceBufferAddress;

        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.geometry.instances = instancesData;

        std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
        VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
        rangeInfo.primitiveCount = static_cast<uint32_t>(instances.size());
        buildRangeInfos.push_back(rangeInfo);

        VkAccelerationStructureBuildGeometryInfoKHR geometryInfo{};
        geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        geometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        geometryInfo.geometryCount = 1;
        geometryInfo.pGeometries = &geometry;
        geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

        std::vector<uint32_t> maxPrimitiveCounts = { static_cast<uint32_t>(instances.size()) };

        return CreateRayTracingAcceleration(
            VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            geometryInfo,
            buildRangeInfos,
            maxPrimitiveCounts,
            desc.PreallocateBytes,
            scratchBuffer
        );
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create ray tracing top acceleration: ") + e.what());
    }
}

VkDeviceAddress Device::FillRayTracingInstanceBuffer(DynamicConstants& dynamicConstants, const std::vector<RayTracingInstanceDesc>& instances) {
    VkDeviceAddress instanceBufferAddress = dynamicConstants.CurrentDeviceAddress(raw_);

    std::vector<GeometryInstance> geometryInstances;
    geometryInstances.reserve(instances.size());

    for (const auto& instanceDesc : instances) {
        VkAccelerationStructureDeviceAddressInfoKHR addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addressInfo.accelerationStructure = instanceDesc.Blas->Raw;

        VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR(raw_, &addressInfo);

        const auto& transform = instanceDesc.Transformation;
        float transformArray[12] = {
            transform[0][0], transform[1][0], transform[2][0], transform[3][0],
            transform[0][1], transform[1][1], transform[2][1], transform[3][1],
            transform[0][2], transform[1][2], transform[2][2], transform[3][2]
        };

        geometryInstances.emplace_back(
            transformArray,
            instanceDesc.MeshIndex, // instance id
            0xff,
            0,
            VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR,
            blasAddress
        );
    }

    dynamicConstants.PushFromIter<GeometryInstance>(geometryInstances.begin(), geometryInstances.end());

    return instanceBufferAddress;
}

void Device::RebuildRayTracingTopAcceleration(VkCommandBuffer commandBuffer, VkDeviceAddress instanceBufferAddress, size_t instanceCount, const std::shared_ptr<RayTracingAcceleration>& tlas, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer) {
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = instanceBufferAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instancesData;

    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos;
    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = static_cast<uint32_t>(instanceCount);
    buildRangeInfos.push_back(rangeInfo);

    VkAccelerationStructureBuildGeometryInfoKHR geometryInfo{};
    geometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    geometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    geometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    geometryInfo.geometryCount = 1;
    geometryInfo.pGeometries = &geometry;
    geometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    std::vector<uint32_t> maxPrimitiveCounts = { static_cast<uint32_t>(instanceCount) };

    RebuildRayTracingAcceleration(
        commandBuffer,
        geometryInfo,
        buildRangeInfos,
        maxPrimitiveCounts,
        tlas,
        scratchBuffer
    );
}

std::shared_ptr<RayTracingAcceleration> Device::CreateRayTracingAcceleration(VkAccelerationStructureTypeKHR type, VkAccelerationStructureBuildGeometryInfoKHR geometryInfo, const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& buildRangeInfos, const std::vector<uint32_t>& maxPrimitiveCounts, size_t preallocateBytes, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer) {
    VkAccelerationStructureBuildSizesInfoKHR memoryRequirements{};
    memoryRequirements.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        raw_,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometryInfo,
        maxPrimitiveCounts.data(),
        &memoryRequirements
    );

    //log::info!(
    //    "Acceleration structure size: {}, scratch size: {}",
    //    memoryRequirements.accelerationStructureSize,
    //    memoryRequirements.buildScratchSize
    //);

    size_t backingBufferSize = std::max(preallocateBytes, static_cast<size_t>(memoryRequirements.accelerationStructureSize));

    BufferDesc accelBufferDesc = BufferDesc::NewGpuOnly(
        backingBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
    );

    auto accelBuffer = CreateBuffer(accelBufferDesc, "Acceleration structure buffer");

    VkAccelerationStructureCreateInfoKHR accelInfo{};
    accelInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    accelInfo.type = type;
    accelInfo.buffer = accelBuffer.Raw;
    accelInfo.size = backingBufferSize;

    std::unique_ptr<Buffer> tmpScratchBuffer;
    std::unique_lock<std::mutex> scratchBufferLock;

    Buffer* scratchBufferPtr = nullptr;

    if (scratchBuffer) {
        scratchBufferLock = std::unique_lock<std::mutex>(*scratchBuffer->BufferMutex);
        scratchBufferPtr = &scratchBuffer->Buffer;
    } else {
        BufferDesc scratchDesc = BufferDesc::NewGpuOnly(
            static_cast<size_t>(memoryRequirements.buildScratchSize),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        ).WithAlignment(256); // TODO: query minAccelerationStructureScratchOffsetAlignment

        tmpScratchBuffer = std::make_unique<Buffer>(CreateBuffer(scratchDesc, "Acceleration structure scratch buffer"));
        scratchBufferPtr = tmpScratchBuffer.get();
    }

    try {
        VkAccelerationStructureKHR accelRaw;
        VkResult result = vkCreateAccelerationStructureKHR(raw_, &accelInfo, nullptr, &accelRaw);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create acceleration structure");
        }

        if (static_cast<size_t>(memoryRequirements.buildScratchSize) > scratchBufferPtr->desc.Size) {
            throw std::runtime_error("TODO: resize scratch; see `RT_SCRATCH_BUFFER_SIZE`");
        }

        geometryInfo.dstAccelerationStructure = accelRaw;

        VkBufferDeviceAddressInfo bufferAddressInfo{};
        bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferAddressInfo.buffer = scratchBufferPtr->Raw;

        geometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(raw_, &bufferAddressInfo);

        WithSetupCb([&](VkCommandBuffer commandBuffer) {
            const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos = buildRangeInfos.data();
            vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &geometryInfo, &pBuildRangeInfos);

            VkMemoryBarrier memoryBarrier{};
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                0,
                1, &memoryBarrier,
                0, nullptr,
                0, nullptr
            );
        });

        auto acceleration = std::make_shared<RayTracingAcceleration>();
        acceleration->Raw = accelRaw;
        acceleration->BackingBuffer = std::move(accelBuffer);

        return acceleration;
    } catch (...) {
        if (tmpScratchBuffer) {
            ImmediateDestroyBuffer(std::move(*tmpScratchBuffer));
        }
        throw;
    }
}

void Device::RebuildRayTracingAcceleration(VkCommandBuffer commandBuffer, VkAccelerationStructureBuildGeometryInfoKHR geometryInfo, const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& buildRangeInfos, const std::vector<uint32_t>& maxPrimitiveCounts, const std::shared_ptr<RayTracingAcceleration>& acceleration, const std::shared_ptr<RayTracingAccelerationScratchBuffer>& scratchBuffer) {
    VkAccelerationStructureBuildSizesInfoKHR memoryRequirements{};
    memoryRequirements.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        raw_,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &geometryInfo,
        maxPrimitiveCounts.data(),
        &memoryRequirements
    );

    if (static_cast<size_t>(memoryRequirements.accelerationStructureSize) > acceleration->BackingBuffer.desc.Size) {
        throw std::runtime_error("todo: backing");
    }

    std::unique_lock<std::mutex> scratchBufferLock(*scratchBuffer->BufferMutex);
    const auto& scratchBufferObj = scratchBuffer->Buffer;

    if (static_cast<size_t>(memoryRequirements.buildScratchSize) > scratchBufferObj.desc.Size) {
        throw std::runtime_error("todo: scratch");
    }

    geometryInfo.dstAccelerationStructure = acceleration->Raw;

    VkBufferDeviceAddressInfo bufferAddressInfo{};
    bufferAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferAddressInfo.buffer = scratchBufferObj.Raw;

    geometryInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(raw_, &bufferAddressInfo);

    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRangeInfos = buildRangeInfos.data();
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &geometryInfo, &pBuildRangeInfos);

    VkMemoryBarrier memoryBarrier{};
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        0,
        1, &memoryBarrier,
        0, nullptr,
        0, nullptr
    );
}

std::shared_ptr<RayTracingShaderTable> Device::CreateRayTracingShaderTable(const RayTracingShaderTableDesc& desc, VkPipeline pipeline) {
    try {
        //log::trace!("Creating ray tracing shader table: {:?}", desc);

        uint32_t shaderGroupHandleSize = rayTracingPipelineProperties_.shaderGroupHandleSize;
        size_t groupCount = desc.RaygenEntryCount + desc.MissEntryCount + desc.HitEntryCount;
        size_t groupHandlesSize = shaderGroupHandleSize * groupCount;

        std::vector<uint8_t> groupHandles(groupHandlesSize);
        VkResult result = vkGetRayTracingShaderGroupHandlesKHR(
            raw_,
            pipeline,
            0,
            static_cast<uint32_t>(groupCount),
            groupHandlesSize,
            groupHandles.data()
        );

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to get ray tracing shader group handles");
        }

        size_t progSize = shaderGroupHandleSize;

        auto createBindingTable = [&](uint32_t entryOffset, uint32_t entryCount) -> std::shared_ptr<Buffer> {
            if (entryCount == 0) {
                return nullptr;
            }

            std::vector<uint8_t> shaderBindingTableData(entryCount * progSize);

            for (size_t dst = 0; dst < entryCount; ++dst) {
                size_t src = dst + entryOffset;
                std::memcpy(
                    &shaderBindingTableData[dst * progSize],
                    &groupHandles[src * shaderGroupHandleSize],
                    shaderGroupHandleSize
                );
            }

            BufferDesc bufferDesc = BufferDesc::NewGpuOnly(
                shaderBindingTableData.size(),
                VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
            );

            return std::make_shared<Buffer>(CreateBuffer(bufferDesc, "SBT sub-buffer", shaderBindingTableData));
        };

        auto raygenShaderBindingTable = createBindingTable(0, desc.RaygenEntryCount);
        auto missShaderBindingTable = createBindingTable(desc.RaygenEntryCount, desc.MissEntryCount);
        auto hitShaderBindingTable = createBindingTable(desc.RaygenEntryCount + desc.MissEntryCount, desc.HitEntryCount);

        auto shaderTable = std::make_shared<RayTracingShaderTable>();

        if (raygenShaderBindingTable) {
            shaderTable->RaygenShaderBindingTableBuffer = raygenShaderBindingTable;
            shaderTable->RaygenShaderBindingTable.deviceAddress = raygenShaderBindingTable->DeviceAddress(raw_);
            shaderTable->RaygenShaderBindingTable.stride = progSize;
            shaderTable->RaygenShaderBindingTable.size = progSize * desc.RaygenEntryCount;
        }

        if (missShaderBindingTable) {
            shaderTable->MissShaderBindingTableBuffer = missShaderBindingTable;
            shaderTable->MissShaderBindingTable.deviceAddress = missShaderBindingTable->DeviceAddress(raw_);
            shaderTable->MissShaderBindingTable.stride = progSize;
            shaderTable->MissShaderBindingTable.size = progSize * desc.MissEntryCount;
        }

        if (hitShaderBindingTable) {
            shaderTable->HitShaderBindingTableBuffer = hitShaderBindingTable;
            shaderTable->HitShaderBindingTable.deviceAddress = hitShaderBindingTable->DeviceAddress(raw_);
            shaderTable->HitShaderBindingTable.stride = progSize;
            shaderTable->HitShaderBindingTable.size = progSize * desc.HitEntryCount;
        }

        shaderTable->CallableShaderBindingTable.deviceAddress = 0;
        shaderTable->CallableShaderBindingTable.stride = 0;
        shaderTable->CallableShaderBindingTable.size = 0;

        return shaderTable;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create ray tracing shader table: ") + e.what());
    }
}

std::shared_ptr<RayTracingPipeline> CreateRayTracingPipeline(const std::shared_ptr<Device>& device, const std::vector<PipelineShader<std::vector<uint32_t>>>& shaders, const RayTracingPipelineDesc& desc) {
    try {
        std::vector<StageDescriptorSetLayouts> stageLayouts;
        stageLayouts.reserve(shaders.size());

        for (const auto& shader : shaders) {
            auto reflection = rspirv_reflect::Reflection::NewFromSpirv(shader.Code);
            if (rspirv_reflect::IsErr(reflection)) {
                throw std::runtime_error("Failed to reflect shader");
            }
            auto descriptorSets = rspirv_reflect::Unwrap(reflection).GetDescriptorSets();
            if (rspirv_reflect::IsErr(descriptorSets)) {
                throw std::runtime_error("Failed to get descriptor sets from shader reflection");
            }
            // Convert rspirv_reflect format to StageDescriptorSetLayouts
            // TODO: Implement conversion
            stageLayouts.push_back(StageDescriptorSetLayouts{});
        }

        //log::info!("{:#?}", stageLayouts);

        auto mergedLayouts = MergeShaderStageLayouts(stageLayouts);
        auto [descriptorSetLayouts, setLayoutInfo] = CreateDescriptorSetLayouts(
            device.get(),
            &mergedLayouts,
            VK_SHADER_STAGE_ALL,
            desc.DescriptorSetOpts
        );

        VkPipelineLayoutCreateInfo layoutCreateInfo{};
        layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        layoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
        layoutCreateInfo.pushConstantRangeCount = 0;
        layoutCreateInfo.pPushConstantRanges = nullptr;

        VkPipelineLayout pipelineLayout;
        VkResult result = vkCreatePipelineLayout(device->raw_, &layoutCreateInfo, nullptr, &pipelineLayout);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        std::vector<std::string> entryPointNames;

        uint32_t raygenEntryCount = 0;
        uint32_t missEntryCount = 0;
        uint32_t hitEntryCount = 0;

        auto createShaderModule = [&](const PipelineShader<std::vector<uint32_t>>& shader) -> std::pair<VkShaderModule, std::string> {
            VkShaderModuleCreateInfo shaderInfo{};
            shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shaderInfo.codeSize = shader.Code.size() * sizeof(uint32_t);
            shaderInfo.pCode = shader.Code.data();

            VkShaderModule shaderModule;
            VkResult res = vkCreateShaderModule(device->raw_, &shaderInfo, nullptr, &shaderModule);
            if (res != VK_SUCCESS) {
                throw std::runtime_error("Failed to create shader module");
            }

            return {shaderModule, shader.Desc.Entry};
        };

        std::optional<ShaderPipelineStage> prevStage;

        for (const auto& shader : shaders) {
            uint32_t groupIdx = static_cast<uint32_t>(shaderStages.size());

            switch (shader.Desc.Stage) {
                case ShaderPipelineStage::RayGen: {
                    if (prevStage.has_value() && prevStage != ShaderPipelineStage::RayGen) {
                        throw std::runtime_error("Invalid shader stage order");
                    }
                    raygenEntryCount++;

                    auto [module, entryPoint] = createShaderModule(shader);
                    entryPointNames.push_back(entryPoint);

                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
                    stage.module = module;
                    stage.pName = entryPointNames.back().c_str();

                    VkRayTracingShaderGroupCreateInfoKHR group{};
                    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                    group.generalShader = groupIdx;
                    group.closestHitShader = VK_SHADER_UNUSED_KHR;
                    group.anyHitShader = VK_SHADER_UNUSED_KHR;
                    group.intersectionShader = VK_SHADER_UNUSED_KHR;

                    shaderStages.push_back(stage);
                    shaderGroups.push_back(group);
                    break;
                }

                case ShaderPipelineStage::RayMiss: {
                    if (prevStage != ShaderPipelineStage::RayGen && prevStage != ShaderPipelineStage::RayMiss) {
                        throw std::runtime_error("Invalid shader stage order");
                    }
                    missEntryCount++;

                    auto [module, entryPoint] = createShaderModule(shader);
                    entryPointNames.push_back(entryPoint);

                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
                    stage.module = module;
                    stage.pName = entryPointNames.back().c_str();

                    VkRayTracingShaderGroupCreateInfoKHR group{};
                    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                    group.generalShader = groupIdx;
                    group.closestHitShader = VK_SHADER_UNUSED_KHR;
                    group.anyHitShader = VK_SHADER_UNUSED_KHR;
                    group.intersectionShader = VK_SHADER_UNUSED_KHR;

                    shaderStages.push_back(stage);
                    shaderGroups.push_back(group);
                    break;
                }

                case ShaderPipelineStage::RayClosestHit: {
                    if (prevStage != ShaderPipelineStage::RayMiss && prevStage != ShaderPipelineStage::RayClosestHit) {
                        throw std::runtime_error("Invalid shader stage order");
                    }
                    hitEntryCount++;

                    auto [module, entryPoint] = createShaderModule(shader);
                    entryPointNames.push_back(entryPoint);

                    VkPipelineShaderStageCreateInfo stage{};
                    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    stage.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
                    stage.module = module;
                    stage.pName = entryPointNames.back().c_str();

                    VkRayTracingShaderGroupCreateInfoKHR group{};
                    group.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
                    group.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
                    group.generalShader = VK_SHADER_UNUSED_KHR;
                    group.closestHitShader = groupIdx;
                    group.anyHitShader = VK_SHADER_UNUSED_KHR;
                    group.intersectionShader = VK_SHADER_UNUSED_KHR;

                    shaderStages.push_back(stage);
                    shaderGroups.push_back(group);
                    break;
                }

                default:
                    throw std::runtime_error("Unimplemented shader stage");
            }

            prevStage = shader.Desc.Stage;
        }

        if (raygenEntryCount == 0) {
            throw std::runtime_error("Ray tracing pipeline must have at least one raygen shader");
        }
        if (missEntryCount == 0) {
            throw std::runtime_error("Ray tracing pipeline must have at least one miss shader");
        }

        VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
        pipelineInfo.pGroups = shaderGroups.data();
        pipelineInfo.maxPipelineRayRecursionDepth = desc.MaxPipelineRayRecursionDepth;
        pipelineInfo.layout = pipelineLayout;

        VkPipeline pipeline;
        auto vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(device->rayTracingPipelineExt_);
        result = vkCreateRayTracingPipelinesKHR(
            device->raw_,
            VK_NULL_HANDLE,
            VK_NULL_HANDLE,
            1,
            &pipelineInfo,
            nullptr,
            &pipeline
        );

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to create ray tracing pipeline");
        }

        std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
        for (const auto& bindings : setLayoutInfo) {
            for (const auto& [binding, type] : bindings) {
                auto it = std::find_if(descriptorPoolSizes.begin(), descriptorPoolSizes.end(),
                    [type](const VkDescriptorPoolSize& dps) { return dps.type == type; });

                if (it != descriptorPoolSizes.end()) {
                    it->descriptorCount += 1;
                } else {
                    descriptorPoolSizes.push_back(VkDescriptorPoolSize{
                        .type = type,
                        .descriptorCount = 1
                    });
                }
            }
        }

        auto sbt = device->CreateRayTracingShaderTable(
            RayTracingShaderTableDesc{
                .RaygenEntryCount = raygenEntryCount,
                .HitEntryCount = hitEntryCount,
                .MissEntryCount = missEntryCount
            },
            pipeline
        );

        auto rtPipeline = std::make_shared<RayTracingPipeline>();
        rtPipeline->Common.PipelineLayout = pipelineLayout;
        rtPipeline->Common.Pipeline = pipeline;
        rtPipeline->Common.SetLayoutInfo = setLayoutInfo;
        rtPipeline->Common.DescriptorPoolSizes = descriptorPoolSizes;
        rtPipeline->Common.DescriptorSetLayouts = descriptorSetLayouts;
        rtPipeline->Common.PipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
        rtPipeline->Sbt = *sbt;

        return rtPipeline;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to create ray tracing pipeline: ") + e.what());
    }
}

} // namespace tekki::backend::vulkan
