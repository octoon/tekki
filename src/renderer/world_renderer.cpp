#include "tekki/renderer/world_renderer.h"
#include "tekki/renderer/image_lut.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>
#include <mutex>
#include <algorithm>
#include <array>
#include <optional>
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/backend/vulkan/ray_tracing.h"
#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vulkan/buffer.h"
#include "tekki/backend/dynamic_constants.h"
#include "tekki/rust_shaders_shared/view_constants.h"
#include "tekki/rust_shaders_shared/frame_constants.h"
#include "tekki/rust_shaders_shared/camera.h"
#include "tekki/rust_shaders_shared/render_overrides.h"
#include "tekki/asset/mesh.h"
#include "tekki/core/result.h"

namespace tekki::renderer {

const InstanceHandle InstanceHandle::Invalid = InstanceHandle(std::numeric_limits<size_t>::max());

float DynamicExposureState::EvSmoothed() const {
    constexpr float DYNAMIC_EXPOSURE_BIAS = -2.0f;
    if (Enabled) {
        return (EvSlow + EvFast) * 0.5f + DYNAMIC_EXPOSURE_BIAS;
    } else {
        return 0.0f;
    }
}

void DynamicExposureState::Update(float ev, float dt) {
    if (!Enabled) {
        return;
    }

    ev = std::clamp(ev, -16.0f, 16.0f);
    dt = dt * std::exp2(SpeedLog2);

    float t_fast = 1.0f - std::exp(-1.0f * dt);
    EvFast = (ev - EvFast) * t_fast + EvFast;

    float t_slow = 1.0f - std::exp(-0.25f * dt);
    EvSlow = (ev - EvSlow) * t_slow + EvSlow;
}

std::shared_ptr<WorldRenderer> WorldRenderer::CreateEmpty(
    const glm::uvec2& renderExtent,
    const glm::uvec2& temporalUpscaleExtent,
    const std::shared_ptr<tekki::backend::vulkan::Device>& device) {

    auto renderer = std::shared_ptr<WorldRenderer>(new WorldRenderer());

    // Initialize render passes
    // TODO: Implement render pass creation

    // Initialize buffers
    renderer->MeshBuffer = device->CreateBuffer(
        tekki::backend::vulkan::BufferDesc::NewCpuToGpu(
            MAX_GPU_MESHES * sizeof(GpuMesh),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        ),
        "mesh buffer"
    );

    renderer->VertexBuffer = device->CreateBuffer(
        tekki::backend::vulkan::BufferDesc::NewGpuOnly(
            VERTEX_BUFFER_CAPACITY,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        ),
        "vertex buffer"
    );

    renderer->BindlessTextureSizes = device->CreateBuffer(
        tekki::backend::vulkan::BufferDesc::NewCpuToGpu(
            device->MaxBindlessDescriptorCount() * sizeof(glm::vec4),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
        ),
        "bindless_texture_sizes"
    );

    // Initialize descriptor sets
    // TODO: Implement bindless descriptor set creation

    // Initialize supersample offsets
    constexpr size_t supersample_count = 128;
    renderer->SupersampleOffsets.reserve(supersample_count);
    for (size_t i = 1; i <= supersample_count; ++i) {
        float x = RadicalInverse(static_cast<uint32_t>(i), 2) - 0.5f;
        float y = RadicalInverse(static_cast<uint32_t>(i), 3) - 0.5f;
        renderer->SupersampleOffsets.emplace_back(x, y);
    }

    // Initialize acceleration structure scratch buffer
    renderer->AccelScratch = device->CreateRayTracingAccelerationScratchBuffer();

    // Initialize renderers
    // TODO: Initialize renderer subsystems properly when classes are defined
    // renderer->Post = PostProcessRenderer(device);
    // renderer->Ssgi = SsgiRenderer();
    // renderer->Rtr = RtrRenderer(device);
    // renderer->Lighting = LightingRenderer();
    // renderer->Ircache = IrcacheRenderer(device);
    // renderer->Rtdgi = RtdgiRenderer();
    // renderer->Taa = TaaRenderer();
    // renderer->ShadowDenoise = ShadowDenoiseRenderer();
    // renderer->Ibl = IblRenderer();

    renderer->Device = device;
    renderer->TemporalUpscaleExtent = temporalUpscaleExtent;
    renderer->FrameIdx = 0;
    renderer->NextInstanceHandle = 0;
    renderer->NextBindlessImageId = 0;
    renderer->VertexBufferWritten = 0;
    renderer->CurrentRenderMode = RenderMode::Standard;
    renderer->ResetReferenceAccumulation = false;
    renderer->UseDlss = false;
    renderer->DebugMode = RenderDebugMode::None;
    renderer->DebugShadingMode = device->RayTracingEnabled() ? 0 : 4;
    renderer->DebugShowWrc = false;
    renderer->EvShift = 0.0f;
    renderer->Contrast = 1.0f;
    renderer->SunSizeMultiplier = 1.0f;
    renderer->SunColorMultiplier = glm::vec3(1.0f);
    renderer->SkyAmbient = glm::vec3(0.0f);

    return renderer;
}

void WorldRenderer::AddImageLut(std::unique_ptr<class ComputeImageLut> computer, size_t id) {
    ImageLut imageLut(Device, std::move(computer));
    ImageLuts.push_back(std::move(imageLut));

    auto view = ImageLuts.back().BackingImage()->GetView(*Device, tekki::backend::vulkan::ImageViewDesc());
    auto handle = AddBindlessImageView(view);

    if (handle.GetValue() != id) {
        throw std::runtime_error("Image LUT ID mismatch");
    }
}

BindlessImageHandle WorldRenderer::AddImage(const std::shared_ptr<tekki::backend::vulkan::Image>& image) {
    auto view = image->GetView(*Device, tekki::backend::vulkan::ImageViewDesc());
    auto handle = AddBindlessImageView(view);

    BindlessImages.push_back(image);

    glm::vec4 imageSize = image->desc.GetExtentInvExtent2d();

    auto mapped_data = BindlessTextureSizes->Allocation.MappedSlice();
    auto sizes = reinterpret_cast<glm::vec4*>(mapped_data);
    sizes[handle.GetValue()] = imageSize;

    return handle;
}

MeshHandle WorldRenderer::AddMesh(const std::shared_ptr<tekki::asset::PackedTriMesh>& mesh, const AddMeshOptions& opts) {
    size_t mesh_idx = Meshes.size();
    
    // Load and process images
    std::vector<std::shared_ptr<tekki::asset::GpuImage>> unique_images;
    // TODO: Extract unique images from mesh
    
    std::vector<std::shared_ptr<tekki::backend::vulkan::Image>> loaded_images;
    for (const auto& image_asset : unique_images) {
        // TODO: Load image from asset
        auto image = std::make_shared<tekki::backend::vulkan::Image>();
        loaded_images.push_back(image);
    }
    
    std::vector<BindlessImageHandle> loaded_image_handles;
    for (const auto& image : loaded_images) {
        loaded_image_handles.push_back(AddImage(image));
    }
    
    // Process materials
    std::vector<tekki::asset::MeshMaterial> materials = mesh->GetMaterials();
    // TODO: Update material map indices
    
    if (opts.UseLights) {
        for (auto& mat : materials) {
            // TODO: Set emissive flag
        }
    }
    
    uint32_t vertex_data_offset = static_cast<uint32_t>(VertexBufferWritten);
    
    // Build vertex buffer data
    // TODO: Implement buffer building
    
    // Upload to GPU
    {
        std::lock_guard<std::mutex> lock(VertexBufferMutex);
        // TODO: Upload vertex data
        VertexBufferWritten += /* total_buffer_size */ 0;
    }
    
    // Update mesh buffer
    {
        std::lock_guard<std::mutex> lock(MeshBufferMutex);
        auto mapped_data = MeshBuffer->Allocation.MappedSlice();
        auto gpu_meshes = reinterpret_cast<GpuMesh*>(mapped_data);

        gpu_meshes[mesh_idx] = GpuMesh{
            /* vertex_core_offset */ 0,
            /* vertex_uv_offset */ 0,
            /* vertex_mat_offset */ 0,
            /* vertex_aux_offset */ 0,
            /* vertex_tangent_offset */ 0,
            /* mat_data_offset */ 0,
            /* index_offset */ 0
        };
    }
    
    // Create BLAS if ray tracing is enabled
    if (Device->RayTracingEnabled()) {
        auto vertex_buffer_da = VertexBuffer->DeviceAddress(Device->GetRaw()) + /* vertex_core_offset */ 0;
        auto index_buffer_da = VertexBuffer->DeviceAddress(Device->GetRaw()) + /* index_offset */ 0;
        
        tekki::backend::vulkan::RayTracingBottomAccelerationDesc blas_desc;
        tekki::backend::vulkan::RayTracingGeometryDesc geometry_desc;
        geometry_desc.GeometryType = tekki::backend::vulkan::RayTracingGeometryType::Triangle;
        geometry_desc.VertexBuffer = vertex_buffer_da;
        geometry_desc.IndexBuffer = index_buffer_da;
        geometry_desc.VertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry_desc.VertexStride = sizeof(tekki::asset::PackedVertex);
        
        tekki::backend::vulkan::RayTracingGeometryPart part;
        part.IndexCount = mesh->GetIndices().size();
        part.IndexOffset = 0;
        part.MaxVertex = *std::max_element(mesh->GetIndices().begin(), mesh->GetIndices().end());
        
        geometry_desc.Parts = {part};
        blas_desc.Geometries = {geometry_desc};
        
        auto blas = Device->CreateRayTracingBottomAcceleration(blas_desc);
        MeshBlas.push_back(std::make_shared<tekki::backend::vulkan::RayTracingAcceleration>(blas));
    }
    
    // Add mesh to list
    Meshes.push_back(UploadedTriMesh{
        /* IndexBufferOffset */ 0,
        /* IndexCount */ static_cast<uint32_t>(mesh->GetIndices().size())
    });
    
    // Process mesh lights if requested
    MeshLightSet mesh_lights;
    if (opts.UseLights) {
        // TODO: Extract emissive triangles as lights
    }
    MeshLights.push_back(std::move(mesh_lights));
    
    return MeshHandle(mesh_idx);
}

InstanceHandle WorldRenderer::AddInstance(MeshHandle mesh, const glm::mat4& transform) {
    size_t handle_value = NextInstanceHandle++;
    InstanceHandle handle(handle_value);
    
    size_t index = Instances.size();
    
    Instances.push_back(MeshInstance{
        transform,
        transform, // prev_transform same as current initially
        mesh,
        InstanceDynamicParameters()
    });
    InstanceHandles.push_back(handle);
    
    InstanceHandleToIndex[handle] = index;
    
    return handle;
}

void WorldRenderer::RemoveInstance(InstanceHandle inst) {
    auto it = InstanceHandleToIndex.find(inst);
    if (it == InstanceHandleToIndex.end()) {
        throw std::runtime_error("No such instance");
    }
    
    size_t index = it->second;
    InstanceHandleToIndex.erase(it);
    
    // Swap remove from instances and handles
    std::swap(Instances[index], Instances.back());
    std::swap(InstanceHandles[index], InstanceHandles.back());
    Instances.pop_back();
    InstanceHandles.pop_back();
    
    // Update mapping for the instance that was moved
    if (index < InstanceHandles.size()) {
        InstanceHandleToIndex[InstanceHandles[index]] = index;
    }
}

void WorldRenderer::SetInstanceTransform(InstanceHandle inst, const glm::mat4& transform) {
    auto it = InstanceHandleToIndex.find(inst);
    if (it == InstanceHandleToIndex.end()) {
        throw std::runtime_error("No such instance");
    }
    
    Instances[it->second].Transform = transform;
}

const InstanceDynamicParameters& WorldRenderer::GetInstanceDynamicParameters(InstanceHandle inst) const {
    auto it = InstanceHandleToIndex.find(inst);
    if (it == InstanceHandleToIndex.end()) {
        throw std::runtime_error("No such instance");
    }
    
    return Instances[it->second].DynamicParameters;
}

InstanceDynamicParameters& WorldRenderer::GetInstanceDynamicParametersMut(InstanceHandle inst) {
    auto it = InstanceHandleToIndex.find(inst);
    if (it == InstanceHandleToIndex.end()) {
        throw std::runtime_error("No such instance");
    }
    
    return Instances[it->second].DynamicParameters;
}

void WorldRenderer::BuildRayTracingTopLevelAcceleration() {
    std::vector<tekki::backend::vulkan::RayTracingInstanceDesc> instances;
    instances.reserve(Instances.size());
    
    for (const auto& inst : Instances) {
        instances.push_back(tekki::backend::vulkan::RayTracingInstanceDesc{
            MeshBlas[inst.Mesh.GetValue()],
            inst.Transform,
            static_cast<uint32_t>(inst.Mesh.GetValue())
        });
    }
    
    tekki::backend::vulkan::RayTracingTopAccelerationDesc tlas_desc;
    tlas_desc.Instances = instances;
    tlas_desc.PreallocateBytes = TLAS_PREALLOCATE_BYTES;
    
    auto tlas = Device->CreateRayTracingTopAcceleration(tlas_desc, AccelScratch);
    Tlas = std::make_shared<tekki::backend::vulkan::RayTracingAcceleration>(tlas);
}

void WorldRenderer::ResetFrameIdx() {
    FrameIdx = 0;
}

void WorldRenderer::PrepareTopLevelAcceleration(class TemporalRenderGraph& rg) {
    if (!Tlas) {
        throw std::runtime_error("TLAS not built");
    }
    
    auto tlas_handle = rg.Import(Tlas, vk_sync::AccessType::AnyShaderReadOther);
    
    std::vector<tekki::backend::vulkan::RayTracingInstanceDesc> instances;
    instances.reserve(Instances.size());
    
    for (const auto& inst : Instances) {
        instances.push_back(tekki::backend::vulkan::RayTracingInstanceDesc{
            MeshBlas[inst.Mesh.GetValue()],
            inst.Transform,
            static_cast<uint32_t>(inst.Mesh.GetValue())
        });
    }
    
    auto pass = rg.AddPass("rebuild tlas");
    auto tlas_ref = pass.Write(tlas_handle, vk_sync::AccessType::TransferWrite);
    
    auto accel_scratch = AccelScratch;
    
    pass.Render([instances, accel_scratch, this](auto& api) {
        auto instance_buffer_address = api.Resources.ExecutionParams.Device->FillRayTracingInstanceBuffer(
            api.Resources.DynamicConstants, instances);
        
        auto tlas_obj = api.Resources.GetRayTracingAcceleration(tlas_ref);
        
        api.Device->RebuildRayTracingTopAcceleration(
            api.CommandBuffer.GetRaw(),
            instance_buffer_address,
            instances.size(),
            tlas_obj,
            accel_scratch
        );
    });
}

ExposureState WorldRenderer::GetExposureState() const {
    return ExposureStates[static_cast<int>(CurrentRenderMode)];
}

void WorldRenderer::PrepareRenderGraph(class TemporalRenderGraph& rg, const WorldFrameDesc& frameDesc) {
    UpdatePreExposure();
    
    // TODO: Set predefined descriptor set layouts
    
    for (auto& image_lut : ImageLuts) {
        image_lut.ComputeIfNeeded(rg);
    }
    
    switch (CurrentRenderMode) {
        case RenderMode::Standard:
            if (USE_TAA_JITTER) {
                if (Taa) {
                    Taa->CurrentSupersampleOffset = SupersampleOffsets[FrameIdx % SupersampleOffsets.size()];
                }
            } else {
                if (Taa) {
                    Taa->CurrentSupersampleOffset = glm::vec2(0.0f);
                }
            }
            PrepareRenderGraphStandard(rg, frameDesc);
            break;

        case RenderMode::Reference:
            if (Taa) {
                Taa->CurrentSupersampleOffset = glm::vec2(0.0f);
            }
            PrepareRenderGraphReference(rg, frameDesc);
            break;
    }
}

struct FrameConstantsLayout WorldRenderer::PrepareFrameConstants(
    tekki::backend::DynamicConstants& dynamicConstants,
    const WorldFrameDesc& frameDesc,
    float deltaTimeSeconds) {
    
    auto prev_camera = PrevCameraMatrices.value_or(frameDesc.CameraMatrices);
    auto view_constants = tekki::rust_shaders_shared::ViewConstants::Builder(
        frameDesc.CameraMatrices,
        prev_camera,
        frameDesc.RenderExtent
    ).Build();

    if (Taa) {
        view_constants.SetPixelOffset(Taa->CurrentSupersampleOffset, frameDesc.RenderExtent);
    }

    // Collect triangle lights
    std::vector<TriangleLight> triangle_lights;
    for (const auto& inst : Instances) {
        glm::vec3 scale, translation;
        glm::quat rotation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(inst.Transform, scale, rotation, translation, skew, perspective);

        glm::vec3 emissive_multiplier(inst.DynamicParameters.EmissiveMultiplier);

        const auto& mesh_light_set = MeshLights[inst.Mesh.GetValue()];
        for (const auto& light : mesh_light_set.Lights) {
            auto transformed_light = light.Transform(translation, glm::mat3_cast(rotation));
            auto scaled_light = transformed_light.ScaleRadiance(emissive_multiplier);
            triangle_lights.push_back(scaled_light);
        }
    }

    // Initialize IR cache cascades
    std::array<tekki::rust_shaders_shared::IrcacheCascadeConstants, IRCACHE_CASCADE_COUNT> ircache_cascades{};

    if (Ircache) {
        Ircache->UpdateEyePosition(view_constants.GetEyePosition());

        const auto& ircache_constants = Ircache->GetConstants();
        for (size_t i = 0; i < ircache_constants.size() && i < IRCACHE_CASCADE_COUNT; ++i) {
            ircache_cascades[i] = ircache_constants[i];
        }
    }

    constexpr float real_sun_angular_radius = glm::radians(0.53f) * 0.5f;

    glm::vec3 ircache_grid_center = Ircache ? Ircache->GetGridCenter() : glm::vec3(0.0f);

    tekki::rust_shaders_shared::FrameConstants frame_constants{
        view_constants,
        glm::vec4(frameDesc.SunDirection, 0.0f),
        FrameIdx,
        deltaTimeSeconds,
        std::cos(SunSizeMultiplier * real_sun_angular_radius),
        glm::vec4(SunColorMultiplier, 0.0f),
        glm::vec4(SkyAmbient, 0.0f),
        static_cast<uint32_t>(triangle_lights.size()),
        GetExposureState().PreMult,
        GetExposureState().PreMultPrev,
        GetExposureState().PreMultDelta,
        0.0f,
        RenderOverrides,
        glm::vec4(ircache_grid_center, 1.0f),
        ircache_cascades
    };
    
    uint32_t globals_offset = dynamicConstants.Push(frame_constants);
    
    std::vector<InstanceDynamicParameters> instance_params;
    instance_params.reserve(Instances.size());
    for (const auto& inst : Instances) {
        instance_params.push_back(inst.DynamicParameters);
    }
    uint32_t instance_dynamic_parameters_offset = dynamicConstants.PushFromIter(instance_params.begin(), instance_params.end());
    
    uint32_t triangle_lights_offset = dynamicConstants.PushFromIter(triangle_lights.begin(), triangle_lights.end());
    
    PrevCameraMatrices = frameDesc.CameraMatrices;
    
    return FrameConstantsLayout{
        globals_offset,
        instance_dynamic_parameters_offset,
        triangle_lights_offset
    };
}

void WorldRenderer::RetireFrame() {
    FrameIdx = (FrameIdx + 1) % std::numeric_limits<uint32_t>::max();
    StorePrevMeshTransforms();
}

void WorldRenderer::WriteDescriptorSetBuffer(
    VkDevice device,
    VkDescriptorSet set,
    uint32_t dstBinding,
    const std::shared_ptr<tekki::backend::vulkan::Buffer>& buffer) {
    
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer->GetRaw();
    buffer_info.range = VK_WHOLE_SIZE;
    
    VkWriteDescriptorSet write_descriptor_set{};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.dstSet = set;
    write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_descriptor_set.dstBinding = dstBinding;
    write_descriptor_set.pBufferInfo = &buffer_info;
    write_descriptor_set.descriptorCount = 1;
    
    vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
}

BindlessImageHandle WorldRenderer::AddBindlessImageView(VkImageView view) {
    uint32_t handle_value = static_cast<uint32_t>(NextBindlessImageId);
    NextBindlessImageId++;
    BindlessImageHandle handle(handle_value);
    
    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = view;
    
    // TODO: Get correct binding index
    constexpr uint32_t BINDLESS_TEXTURES_BINDING_INDEX = 3;
    
    VkWriteDescriptorSet write_descriptor_set{};
    write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_descriptor_set.dstSet = BindlessDescriptorSet;
    write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write_descriptor_set.dstBinding = BINDLESS_TEXTURES_BINDING_INDEX;
    write_descriptor_set.dstArrayElement = handle_value;
    write_descriptor_set.pImageInfo = &image_info;
    write_descriptor_set.descriptorCount = 1;
    
    vkUpdateDescriptorSets(Device->GetRaw(), 1, &write_descriptor_set, 0, nullptr);
    
    return handle;
}

void WorldRenderer::StorePrevMeshTransforms() {
    for (auto& inst : Instances) {
        inst.PrevTransform = inst.Transform;
    }
}

void WorldRenderer::UpdatePreExposure() {
    constexpr float dt = 1.0f / 60.0f; // TODO: Use actual delta time

    float image_log2_lum = Post ? Post->GetImageLog2Lum() : 0.0f;
    DynamicExposure.Update(-image_log2_lum, dt);
    float ev_mult = std::exp2(EvShift + DynamicExposure.EvSmoothed());
    
    auto& exposure_state = ExposureStates[static_cast<int>(CurrentRenderMode)];
    exposure_state.PreMultPrev = exposure_state.PreMult;
    
    switch (CurrentRenderMode) {
        case RenderMode::Standard:
            exposure_state.PreMult = exposure_state.PreMult * 0.9f + ev_mult * 0.1f;
            exposure_state.PostMult = ev_mult / exposure_state.PreMult;
            break;
            
        case RenderMode::Reference:
            exposure_state.PreMult = 1.0f;
            exposure_state.PostMult = ev_mult;
            break;
    }
    
    exposure_state.PreMultDelta = exposure_state.PreMult / exposure_state.PreMultPrev;
}

class ImageHandle WorldRenderer::PrepareRenderGraphStandard(class TemporalRenderGraph& rg, const WorldFrameDesc& frameDesc) {
    // TODO: Implement standard rendering path
    return ImageHandle();
}

class ImageHandle WorldRenderer::PrepareRenderGraphReference(class TemporalRenderGraph& rg, const WorldFrameDesc& frameDesc) {
    // TODO: Implement reference rendering path
    return ImageHandle();
}

float RadicalInverse(uint32_t n, uint32_t base) {
    float val = 0.0f;
    float inv_base = 1.0f / static_cast<float>(base);
    float inv_bi = inv_base;
    
    while (n > 0) {
        uint32_t d_i = n % base;
        val += static_cast<float>(d_i) * inv_bi;
        n = static_cast<uint32_t>(static_cast<float>(n) * inv_base);
        inv_bi *= inv_base;
    }
    
    return val;
}

} // namespace tekki::renderer