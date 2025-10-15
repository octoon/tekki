#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "tekki/render_graph/pass_builder.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vulkan/ray_tracing.h"

namespace tekki::render_graph {

class SimpleRenderPass {
public:
    // Compute pass constructors
    static SimpleRenderPass NewCompute(
        PassBuilder& pass,
        const std::string& pipeline_path
    );

    static SimpleRenderPass NewComputeRust(
        PassBuilder& pass,
        const std::string& entry_name
    );

    // Ray tracing pass constructor
    static SimpleRenderPass NewRt(
        PassBuilder& pass,
        const tekki::backend::vulkan::ShaderSource& rgen,
        const std::vector<tekki::backend::vulkan::ShaderSource>& miss,
        const std::vector<tekki::backend::vulkan::ShaderSource>& hit
    );

    // Alias for NewRt for compatibility
    static SimpleRenderPass NewRayTracing(
        PassBuilder& pass,
        const tekki::backend::vulkan::ShaderSource& rgen,
        const std::vector<tekki::backend::vulkan::ShaderSource>& miss,
        const std::vector<tekki::backend::vulkan::ShaderSource>& hit
    ) {
        return NewRt(pass, rgen, miss, hit);
    }

    // Resource binding methods
    template<typename Res>
    SimpleRenderPass& Read(const Handle<Res>& handle) {
        pass_.Read(handle, vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);
        return *this;
    }

    template<typename Res>
    SimpleRenderPass& ReadAspect(const Handle<Res>& handle, VkImageAspectFlags aspect) {
        // Note: This is a simplified version - in Rust this would use read_view
        pass_.Read(handle, vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer);
        return *this;
    }

    template<typename Res>
    SimpleRenderPass& Write(Handle<Res>& handle) {
        pass_.Write(handle, vk_sync::AccessType::ComputeShaderWrite);
        return *this;
    }

    // Constants binding
    template<typename... Args>
    SimpleRenderPass& Constants(const std::tuple<Args...>& constants) {
        // Store constants for later use
        constants_ = std::make_shared<std::tuple<Args...>>(constants);
        return *this;
    }

    // Raw descriptor set binding
    SimpleRenderPass& RawDescriptorSet(uint32_t set_idx, VkDescriptorSet descriptor_set) {
        raw_descriptor_sets_.emplace_back(set_idx, descriptor_set);
        return *this;
    }

    // Dispatch methods
    void Dispatch(const glm::u32vec3& extent);
    void Dispatch(const glm::u32vec2& extent);

    // Ray tracing dispatch
    void TraceRays(
        const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
        const glm::u32vec3& extent
    );

    void TraceRays(
        const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
        const glm::u32vec2& extent
    ) {
        TraceRays(tlas, glm::u32vec3(extent.x, extent.y, 1));
    }

    void TraceRays(
        const std::shared_ptr<tekki::backend::vulkan::RayTracingAcceleration>& tlas,
        const std::array<uint32_t, 3>& extent
    ) {
        TraceRays(tlas, glm::u32vec3(extent[0], extent[1], extent[2]));
    }

    // Additional methods for specific renderers
    template<typename T>
    SimpleRenderPass& Bind(const T& render_state) {
        // Placeholder for render state binding
        return *this;
    }

    template<typename T>
    SimpleRenderPass& BindMut(T& render_state) {
        // Placeholder for mutable render state binding
        return *this;
    }

private:
    SimpleRenderPass(PassBuilder&& pass);

    PassBuilder pass_;
    std::shared_ptr<void> constants_;
    std::vector<std::pair<uint32_t, VkDescriptorSet>> raw_descriptor_sets_;
};

} // namespace tekki::render_graph