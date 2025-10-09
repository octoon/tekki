// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "../../include/tekki/backend/spirv_reflect_helper.h"
#include <spdlog/spdlog.h>
#include <spirv_reflect.h>
#include <stdexcept>

namespace tekki::backend
{

SPIRVReflection::SPIRVReflection(const std::vector<uint32_t>& spirv)
    : SPIRVReflection(spirv.data(), spirv.size() * sizeof(uint32_t))
{
}

SPIRVReflection::SPIRVReflection(const uint32_t* spirv, size_t size_bytes)
{
    module_ = std::make_unique<SpvReflectShaderModule>();

    SpvReflectResult result = spvReflectCreateShaderModule(size_bytes, spirv, module_.get());

    if (result != SPV_REFLECT_RESULT_SUCCESS)
    {
        throw std::runtime_error("Failed to create SPIRV-Reflect module: " + std::to_string(result));
    }
}

SPIRVReflection::~SPIRVReflection()
{
    if (module_)
    {
        spvReflectDestroyShaderModule(module_.get());
    }
}

vk::DescriptorType SPIRVReflection::ConvertDescriptorType(uint32_t spirv_type)
{
    switch (spirv_type)
    {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return vk::DescriptorType::eSampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return vk::DescriptorType::eCombinedImageSampler;
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return vk::DescriptorType::eSampledImage;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return vk::DescriptorType::eStorageImage;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return vk::DescriptorType::eUniformTexelBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return vk::DescriptorType::eStorageTexelBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return vk::DescriptorType::eUniformBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return vk::DescriptorType::eStorageBuffer;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return vk::DescriptorType::eUniformBufferDynamic;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return vk::DescriptorType::eStorageBufferDynamic;
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return vk::DescriptorType::eInputAttachment;
    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return vk::DescriptorType::eAccelerationStructureKHR;
    default:
        spdlog::warn("Unknown SPIRV-Reflect descriptor type: {}", spirv_type);
        return vk::DescriptorType::eUniformBuffer;
    }
}

vk::ShaderStageFlagBits SPIRVReflection::ConvertShaderStage(uint32_t spirv_stage)
{
    switch (spirv_stage)
    {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
        return vk::ShaderStageFlagBits::eVertex;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return vk::ShaderStageFlagBits::eTessellationControl;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return vk::ShaderStageFlagBits::eTessellationEvaluation;
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
        return vk::ShaderStageFlagBits::eGeometry;
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
        return vk::ShaderStageFlagBits::eFragment;
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
        return vk::ShaderStageFlagBits::eCompute;
    case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
        return vk::ShaderStageFlagBits::eRaygenKHR;
    case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return vk::ShaderStageFlagBits::eAnyHitKHR;
    case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return vk::ShaderStageFlagBits::eClosestHitKHR;
    case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
        return vk::ShaderStageFlagBits::eMissKHR;
    case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
        return vk::ShaderStageFlagBits::eIntersectionKHR;
    case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:
        return vk::ShaderStageFlagBits::eCallableKHR;
    default:
        spdlog::warn("Unknown SPIRV-Reflect shader stage: {}", spirv_stage);
        return vk::ShaderStageFlagBits::eVertex;
    }
}

std::unordered_map<uint32_t, std::unordered_map<uint32_t, DescriptorBindingInfo>>
SPIRVReflection::GetDescriptorSets() const
{
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, DescriptorBindingInfo>> result;

    // Enumerate descriptor sets
    uint32_t set_count = 0;
    SpvReflectResult spirv_result = spvReflectEnumerateDescriptorSets(module_.get(), &set_count, nullptr);

    if (spirv_result != SPV_REFLECT_RESULT_SUCCESS || set_count == 0)
    {
        return result;
    }

    std::vector<SpvReflectDescriptorSet*> sets(set_count);
    spirv_result = spvReflectEnumerateDescriptorSets(module_.get(), &set_count, sets.data());

    if (spirv_result != SPV_REFLECT_RESULT_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate descriptor sets");
    }

    // Process each descriptor set
    for (const auto* set : sets)
    {
        uint32_t set_index = set->set;
        auto& binding_map = result[set_index];

        // Process each binding in the set
        for (uint32_t b = 0; b < set->binding_count; ++b)
        {
            const auto* binding = set->bindings[b];

            DescriptorBindingInfo info;
            info.set = set_index;
            info.binding = binding->binding;
            info.name = binding->name ? binding->name : "";
            info.type = ConvertDescriptorType(binding->descriptor_type);

            // Determine count and dimensionality
            if (binding->array.dims_count == 0)
            {
                // Single descriptor
                info.count = 1;
                info.dimensionality = DescriptorDimensionality::Single;
            }
            else if (binding->array.dims_count == 1)
            {
                if (binding->array.dims[0] == 0)
                {
                    // Runtime array (bindless)
                    info.count = 0; // Will be set at pipeline creation
                    info.dimensionality = DescriptorDimensionality::RuntimeArray;
                }
                else
                {
                    // Fixed-size array
                    info.count = binding->array.dims[0];
                    info.dimensionality = DescriptorDimensionality::Array;
                }
            }
            else
            {
                // Multi-dimensional array - treat as single with total count
                info.count = 1;
                for (uint32_t d = 0; d < binding->array.dims_count; ++d)
                {
                    info.count *= binding->array.dims[d];
                }
                info.dimensionality = DescriptorDimensionality::Array;
            }

            binding_map[info.binding] = info;
        }
    }

    return result;
}

std::vector<vk::PushConstantRange> SPIRVReflection::GetPushConstantRanges() const
{
    std::vector<vk::PushConstantRange> result;

    uint32_t count = 0;
    SpvReflectResult spirv_result = spvReflectEnumeratePushConstantBlocks(module_.get(), &count, nullptr);

    if (spirv_result != SPV_REFLECT_RESULT_SUCCESS || count == 0)
    {
        return result;
    }

    std::vector<SpvReflectBlockVariable*> blocks(count);
    spirv_result = spvReflectEnumeratePushConstantBlocks(module_.get(), &count, blocks.data());

    if (spirv_result != SPV_REFLECT_RESULT_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate push constant blocks");
    }

    for (const auto* block : blocks)
    {
        vk::PushConstantRange range{};
        range.stageFlags = ConvertShaderStage(module_->shader_stage);
        range.offset = block->offset;
        range.size = block->size;

        result.push_back(range);
    }

    return result;
}

vk::ShaderStageFlagBits SPIRVReflection::GetShaderStage() const
{
    return ConvertShaderStage(module_->shader_stage);
}

std::string SPIRVReflection::GetEntryPointName() const
{
    return module_->entry_point_name ? module_->entry_point_name : "main";
}

std::optional<std::array<uint32_t, 3>> SPIRVReflection::GetComputeLocalSize() const
{
    if (module_->shader_stage != SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT)
    {
        return std::nullopt;
    }

    return std::array<uint32_t, 3>{
        module_->entry_points[0].local_size.x,
        module_->entry_points[0].local_size.y,
        module_->entry_points[0].local_size.z,
    };
}

} // namespace tekki::backend
