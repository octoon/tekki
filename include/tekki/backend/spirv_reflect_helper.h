// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

// Forward declare SPIRV-Reflect types
struct SpvReflectShaderModule;

namespace tekki::backend
{

// Descriptor dimensionality (for bindless support)
enum class DescriptorDimensionality
{
    Single,      // Normal descriptor
    Array,       // Fixed-size array
    RuntimeArray // Bindless runtime array
};

// Descriptor binding information
struct DescriptorBindingInfo
{
    uint32_t set;
    uint32_t binding;
    vk::DescriptorType type;
    uint32_t count;
    std::string name;
    DescriptorDimensionality dimensionality;

    DescriptorBindingInfo() = default;
};

// SPIRV-Reflect wrapper
class SPIRVReflection
{
  public:
    // Create from SPIR-V bytecode
    explicit SPIRVReflection(const std::vector<uint32_t>& spirv);
    explicit SPIRVReflection(const uint32_t* spirv, size_t size);
    ~SPIRVReflection();

    // Prevent copying
    SPIRVReflection(const SPIRVReflection&) = delete;
    SPIRVReflection& operator=(const SPIRVReflection&) = delete;

    // Get all descriptor set bindings
    // Returns: map<set_index, map<binding_index, DescriptorBindingInfo>>
    std::unordered_map<uint32_t, std::unordered_map<uint32_t, DescriptorBindingInfo>> GetDescriptorSets() const;

    // Get push constant ranges
    std::vector<vk::PushConstantRange> GetPushConstantRanges() const;

    // Get shader stage
    vk::ShaderStageFlagBits GetShaderStage() const;

    // Get entry point name
    std::string GetEntryPointName() const;

    // Get compute shader local size (for compute shaders only)
    std::optional<std::array<uint32_t, 3>> GetComputeLocalSize() const;

  private:
    std::unique_ptr<SpvReflectShaderModule> module_;

    // Convert SPIRV-Reflect descriptor type to Vulkan
    static vk::DescriptorType ConvertDescriptorType(uint32_t spirv_type);

    // Convert SPIRV-Reflect shader stage to Vulkan
    static vk::ShaderStageFlagBits ConvertShaderStage(uint32_t spirv_stage);
};

} // namespace tekki::backend
