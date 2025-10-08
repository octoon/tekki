// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/vulkan/ray_tracing.rs

#pragma once

#include <vulkan/vulkan.hpp>
#include <memory>
#include <vector>

#include "core/common.h"

namespace tekki::backend::vulkan {

struct AccelerationStructureBuildInfo {
    vk::AccelerationStructureTypeKHR type;
    vk::BuildAccelerationStructureFlagsKHR flags;
    vk::BuildAccelerationStructureModeKHR mode;
    vk::AccelerationStructureBuildGeometryInfoKHR geometry_info;
    std::vector<vk::AccelerationStructureBuildRangeInfoKHR> range_infos;
};

class AccelerationStructure {
public:
    AccelerationStructure(
        vk::Device device,
        vk::AccelerationStructureKHR as,
        vk::Buffer buffer,
        vk::DeviceAddress device_address
    );
    ~AccelerationStructure();

    vk::AccelerationStructureKHR raw() const { return acceleration_structure_; }
    vk::DeviceAddress device_address() const { return device_address_; }

private:
    vk::Device device_;
    vk::AccelerationStructureKHR acceleration_structure_;
    std::shared_ptr<class Buffer> buffer_;
    vk::DeviceAddress device_address_;
};

class RayTracingPipeline {
public:
    struct ShaderGroup {
        vk::RayTracingShaderGroupTypeKHR type;
        uint32_t general_shader;
        uint32_t closest_hit_shader;
        uint32_t any_hit_shader;
        uint32_t intersection_shader;
    };

    RayTracingPipeline(
        vk::Device device,
        vk::Pipeline pipeline,
        vk::PipelineLayout layout,
        const std::vector<ShaderGroup>& shader_groups
    );
    ~RayTracingPipeline();

    vk::Pipeline raw() const { return pipeline_; }
    vk::PipelineLayout layout() const { return layout_; }
    const std::vector<ShaderGroup>& shader_groups() const { return shader_groups_; }

    // SBT (Shader Binding Table) creation
    std::shared_ptr<class Buffer> create_shader_binding_table(
        vk::Device device,
        const class Device& backend_device
    ) const;

private:
    vk::Device device_;
    vk::Pipeline pipeline_;
    vk::PipelineLayout layout_;
    std::vector<ShaderGroup> shader_groups_;
};

class RayTracingBuilder {
public:
    static std::shared_ptr<AccelerationStructure> build_blas(
        const class Device& device,
        const AccelerationStructureBuildInfo& build_info
    );

    static std::shared_ptr<AccelerationStructure> build_tlas(
        const class Device& device,
        const AccelerationStructureBuildInfo& build_info
    );

    static std::shared_ptr<RayTracingPipeline> create_pipeline(
        const class Device& device,
        const std::vector<vk::PipelineShaderStageCreateInfo>& stages,
        const std::vector<RayTracingPipeline::ShaderGroup>& groups,
        vk::PipelineLayout layout
    );
};

} // namespace tekki::backend::vulkan