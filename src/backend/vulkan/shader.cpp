#include "tekki/backend/vulkan/shader.h"
#include "tekki/backend/vulkan/device.h"
#include "tekki/backend/vulkan/image.h"
#include "tekki/shader_compiler.h"
#include <array>
#include <vector>
#include <unordered_map>
#include <memory>
#include <string>
#include <optional>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <rspirv_reflect.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cstring>

namespace tekki::backend::vulkan {

std::pair<std::vector<VkDescriptorSetLayout>, std::vector<std::unordered_map<uint32_t, VkDescriptorType>>>
CreateDescriptorSetLayouts(
    Device* device,
    const StageDescriptorSetLayouts* descriptor_sets,
    VkShaderStageFlags stage_flags,
    const std::array<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>, MAX_DESCRIPTOR_SETS>& set_opts) {
    
    std::vector<std::optional<std::pair<uint32_t, DescriptorSetLayoutOpts>>> temp_opts;
    temp_opts.reserve(MAX_DESCRIPTOR_SETS);
    for (const auto& opt : set_opts) {
        temp_opts.push_back(opt);
    }

    uint32_t set_count = 0;
    if (descriptor_sets && !descriptor_sets->empty()) {
        for (const auto& [set_index, _] : *descriptor_sets) {
            set_count = std::max(set_count, set_index + 1);
        }
    }

    for (const auto& opt : temp_opts) {
        if (opt.has_value()) {
            set_count = std::max(set_count, opt->first + 1);
        }
    }

    std::vector<VkDescriptorSetLayout> set_layouts;
    std::vector<std::unordered_map<uint32_t, VkDescriptorType>> set_layout_info;
    set_layouts.reserve(set_count);
    set_layout_info.reserve(set_count);

    for (uint32_t set_index = 0; set_index < set_count; ++set_index) {
        VkShaderStageFlags current_stage_flags = (set_index == 0) ? stage_flags : VK_SHADER_STAGE_ALL;

        DescriptorSetLayoutOpts default_opts;
        const DescriptorSetLayoutOpts* resolved_opts = &default_opts;

        for (auto& opt : temp_opts) {
            if (opt.has_value() && opt->first == set_index) {
                resolved_opts = &opt->second;
                break;
            }
        }

        const DescriptorSetLayout* set = nullptr;
        if (resolved_opts->Replace.has_value()) {
            set = &resolved_opts->Replace.value();
        } else if (descriptor_sets) {
            auto it = descriptor_sets->find(set_index);
            if (it != descriptor_sets->end()) {
                set = &it->second;
            }
        }

        if (set && !set->empty()) {
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.reserve(set->size());
            std::vector<VkDescriptorBindingFlags> binding_flags(set->size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT);
            VkDescriptorSetLayoutCreateFlags set_layout_create_flags = 0;

            for (const auto& [binding_index, binding] : *set) {
                switch (binding.ty) {
                    case rspirv_reflect::DescriptorType::UNIFORM_BUFFER:
                    case rspirv_reflect::DescriptorType::UNIFORM_TEXEL_BUFFER:
                    case rspirv_reflect::DescriptorType::STORAGE_IMAGE:
                    case rspirv_reflect::DescriptorType::STORAGE_BUFFER:
                    case rspirv_reflect::DescriptorType::STORAGE_BUFFER_DYNAMIC: {
                        VkDescriptorType descriptor_type;
                        switch (binding.ty) {
                            case rspirv_reflect::DescriptorType::UNIFORM_BUFFER:
                                descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
                                break;
                            case rspirv_reflect::DescriptorType::UNIFORM_TEXEL_BUFFER:
                                descriptor_type = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
                                break;
                            case rspirv_reflect::DescriptorType::STORAGE_IMAGE:
                                descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
                                break;
                            case rspirv_reflect::DescriptorType::STORAGE_BUFFER:
                                if (binding.name.ends_with("_dyn")) {
                                    descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                                } else {
                                    descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                                }
                                break;
                            case rspirv_reflect::DescriptorType::STORAGE_BUFFER_DYNAMIC:
                                descriptor_type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
                                break;
                            default:
                                throw std::runtime_error("Unimplemented descriptor type");
                        }

                        bindings.push_back(VkDescriptorSetLayoutBinding{
                            .binding = binding_index,
                            .descriptorType = descriptor_type,
                            .descriptorCount = 1,
                            .stageFlags = current_stage_flags,
                            .pImmutableSamplers = nullptr
                        });
                        break;
                    }
                    case rspirv_reflect::DescriptorType::SAMPLED_IMAGE: {
                        if (binding.dimensionality == rspirv_reflect::DescriptorDimensionality::RuntimeArray) {
                            size_t binding_idx = bindings.size();
                            if (binding_idx < binding_flags.size()) {
                                binding_flags[binding_idx] = VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT |
                                                             VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                             VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                            }
                            set_layout_create_flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
                        }

                        uint32_t descriptor_count = 1;
                        switch (binding.dimensionality) {
                            case rspirv_reflect::DescriptorDimensionality::Single:
                                descriptor_count = 1;
                                break;
                            case rspirv_reflect::DescriptorDimensionality::Array:
                                descriptor_count = binding.count;
                                break;
                            case rspirv_reflect::DescriptorDimensionality::RuntimeArray:
                                descriptor_count = device->GetMaxBindlessDescriptorCount();
                                break;
                        }

                        bindings.push_back(VkDescriptorSetLayoutBinding{
                            .binding = binding_index,
                            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                            .descriptorCount = descriptor_count,
                            .stageFlags = current_stage_flags,
                            .pImmutableSamplers = nullptr
                        });
                        break;
                    }
                    case rspirv_reflect::DescriptorType::SAMPLER: {
                        std::string name_prefix = "sampler_";
                        if (binding.name.rfind(name_prefix, 0) == 0) {
                            std::string spec = binding.name.substr(name_prefix.length());
                            if (spec.empty()) {
                                throw std::runtime_error("Invalid sampler name");
                            }

                            VkFilter texel_filter;
                            switch (spec[0]) {
                                case 'n': texel_filter = VK_FILTER_NEAREST; break;
                                case 'l': texel_filter = VK_FILTER_LINEAR; break;
                                default: throw std::runtime_error("Invalid texel filter");
                            }
                            spec = spec.substr(1);

                            VkSamplerMipmapMode mipmap_mode;
                            switch (spec[0]) {
                                case 'n': mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST; break;
                                case 'l': mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR; break;
                                default: throw std::runtime_error("Invalid mipmap mode");
                            }
                            spec = spec.substr(1);

                            VkSamplerAddressMode address_mode;
                            if (spec == "r") {
                                address_mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                            } else if (spec == "mr") {
                                address_mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                            } else if (spec == "c") {
                                address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                            } else if (spec == "cb") {
                                address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                            } else {
                                throw std::runtime_error("Invalid address mode");
                            }

                            SamplerDesc sampler_desc{
                                .TexelFilter = texel_filter,
                                .MipmapMode = mipmap_mode,
                                .AddressModes = address_mode
                            };
                            VkSampler sampler = device->GetSampler(sampler_desc);

                            bindings.push_back(VkDescriptorSetLayoutBinding{
                                .binding = binding_index,
                                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                                .descriptorCount = 1,
                                .stageFlags = current_stage_flags,
                                .pImmutableSamplers = &sampler
                            });
                        } else {
                            throw std::runtime_error("Invalid sampler name: " + binding.name);
                        }
                        break;
                    }
                    case rspirv_reflect::DescriptorType::ACCELERATION_STRUCTURE_KHR:
                        bindings.push_back(VkDescriptorSetLayoutBinding{
                            .binding = binding_index,
                            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                            .descriptorCount = 1,
                            .stageFlags = current_stage_flags,
                            .pImmutableSamplers = nullptr
                        });
                        break;
                    default:
                        throw std::runtime_error("Unimplemented descriptor type");
                }
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo binding_flags_create_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .pNext = nullptr,
                .bindingCount = static_cast<uint32_t>(binding_flags.size()),
                .pBindingFlags = binding_flags.data()
            };

            VkDescriptorSetLayoutCreateInfo layout_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &binding_flags_create_info,
                .flags = (resolved_opts->Flags.value_or(0) | set_layout_create_flags),
                .bindingCount = static_cast<uint32_t>(bindings.size()),
                .pBindings = bindings.data()
            };

            VkDescriptorSetLayout set_layout;
            VkResult result = vkCreateDescriptorSetLayout(device->GetRawDevice(), &layout_info, nullptr, &set_layout);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create descriptor set layout");
            }

            set_layouts.push_back(set_layout);
            
            std::unordered_map<uint32_t, VkDescriptorType> layout_map;
            for (const auto& binding : bindings) {
                layout_map[binding.binding] = binding.descriptorType;
            }
            set_layout_info.push_back(std::move(layout_map));
        } else {
            VkDescriptorSetLayoutCreateInfo layout_info{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .bindingCount = 0,
                .pBindings = nullptr
            };

            VkDescriptorSetLayout set_layout;
            VkResult result = vkCreateDescriptorSetLayout(device->GetRawDevice(), &layout_info, nullptr, &set_layout);
            if (result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create descriptor set layout");
            }

            set_layouts.push_back(set_layout);
            set_layout_info.push_back({});
        }
    }

    return {set_layouts, set_layout_info};
}

ComputePipeline CreateComputePipeline(
    Device* device,
    const std::vector<uint8_t>& spirv,
    const ComputePipelineDesc& desc) {
    
    rspirv_reflect::Reflection reflection;
    if (!reflection.InitFromSpirv(spirv.data(), spirv.size())) {
        throw std::runtime_error("Failed to reflect SPIR-V");
    }

    StageDescriptorSetLayouts descriptor_sets;
    if (!reflection.GetDescriptorSets(&descriptor_sets)) {
        throw std::runtime_error("Failed to get descriptor sets from reflection");
    }

    auto [descriptor_set_layouts, set_layout_info] = CreateDescriptorSetLayouts(
        device, &descriptor_sets, VK_SHADER_STAGE_COMPUTE_BIT, desc.DescriptorSetOpts);

    VkPipelineLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = static_cast<uint32_t>(desc.PushConstantsBytes)
    };

    if (desc.PushConstantsBytes > 0) {
        layout_create_info.pushConstantRangeCount = 1;
        layout_create_info.pPushConstantRanges = &push_constant_range;
    }

    VkDevice vk_device = device->GetRawDevice();
    
    VkShaderModule shader_module;
    VkShaderModuleCreateInfo shader_module_info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = spirv.size(),
        .pCode = reinterpret_cast<const uint32_t*>(spirv.data())
    };
    
    VkResult result = vkCreateShaderModule(vk_device, &shader_module_info, nullptr, &shader_module);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module");
    }

    std::string entry_name = desc.Source.GetEntry();
    VkPipelineShaderStageCreateInfo stage_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader_module,
        .pName = entry_name.c_str(),
        .pSpecializationInfo = nullptr
    };

    VkPipelineLayout pipeline_layout;
    result = vkCreatePipelineLayout(vk_device, &layout_create_info, nullptr, &pipeline_layout);
    if (result != VK_SUCCESS) {
        vkDestroyShaderModule(vk_device, shader_module, nullptr);
        throw std::runtime_error("Failed to create pipeline layout");
    }

    VkComputePipelineCreateInfo pipeline_info{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = stage_create_info,
        .layout = pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline;
    result = vkCreateComputePipelines(vk_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);
    if (result != VK_SUCCESS) {
        vkDestroyPipelineLayout(vk_device, pipeline_layout, nullptr);
        vkDestroyShaderModule(vk_device, shader_module, nullptr);
        throw std::runtime_error("Failed to create compute pipeline");
    }

    vkDestroyShaderModule(vk_device, shader_module, nullptr);

    std::vector<VkDescriptorPoolSize> descriptor_pool_sizes;
    for (const auto& bindings : set_layout_info) {
        for (const auto& [_, type] : bindings) {
            auto it = std::find_if(descriptor_pool_sizes.begin(), descriptor_pool_sizes.end(),
                [type](const VkDescriptorPoolSize& dps) { return dps.type == type; });
            
            if (it != descriptor_pool_sizes.end()) {
                it->descriptorCount += 1;
            } else {
                descriptor_pool_sizes.push_back(VkDescriptorPoolSize{
                    .type = type,
                    .descriptorCount = 1
                });
            }
        }
    }

    std::array<uint32_t, 3> group_size;
    if (!GetCsLocalSizeFromSpirv(reinterpret_cast<const uint32_t*>(spirv.data()), spirv.size() / sizeof(uint32_t), group_size.data())) {
        group_size = {1, 1, 1};
    }

    return ComputePipeline{
        .Common = ShaderPipelineCommon{
            .PipelineLayout = pipeline_layout,
            .Pipeline = pipeline,
            .SetLayoutInfo = set_layout_info,
            .DescriptorPoolSizes = descriptor_pool_sizes,
            .DescriptorSetLayouts = descriptor_set_layouts,
            .PipelineBindPoint = VK_PIPELINE_BIND_POINT_COMPUTE
        },
        .GroupSize = group_size
    };
}

std::shared_ptr<RenderPass> CreateRenderPass(
    Device* device,
    const RenderPassDesc& desc) {
    
    std::vector<VkAttachmentDescription> renderpass_attachments;
    renderpass_attachments.reserve(desc.ColorAttachments->size() + (desc.DepthAttachment.has_value() ? 1 : 0));

    for (const auto& attachment : *desc.ColorAttachments) {
        renderpass_attachments.push_back(attachment.ToVk(
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
    }

    if (desc.DepthAttachment.has_value()) {
        renderpass_attachments.push_back(desc.DepthAttachment->ToVk(
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL));
    }

    std::vector<VkAttachmentReference> color_attachment_refs;
    color_attachment_refs.reserve(desc.ColorAttachments->size());
    for (uint32_t i = 0; i < desc.ColorAttachments->size(); ++i) {
        color_attachment_refs.push_back(VkAttachmentReference{
            .attachment = i,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        });
    }

    VkAttachmentReference depth_attachment_ref{
        .attachment = static_cast<uint32_t>(desc.ColorAttachments->size()),
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
    };

    VkSubpassDescription subpass_description{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = static_cast<uint32_t>(color_attachment_refs.size()),
        .pColorAttachments = color_attachment_refs.data(),
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = desc.DepthAttachment.has_value() ? &depth_attachment_ref : nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr
    };

    std::vector<VkSubpassDescription> subpasses = {subpass_description};

    VkRenderPassCreateInfo render_pass_create_info{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = static_cast<uint32_t>(renderpass_attachments.size()),
        .pAttachments = renderpass_attachments.data(),
        .subpassCount = static_cast<uint32_t>(subpasses.size()),
        .pSubpasses = subpasses.data(),
        .dependencyCount = 0,
        .pDependencies = nullptr
    };

    VkRenderPass render_pass;
    VkResult result = vkCreateRenderPass(device->GetRawDevice(), &render_pass_create_info, nullptr, &render_pass);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass");
    }

    FramebufferCache framebuffer_cache(render_pass, *desc.ColorAttachments, desc.DepthAttachment);
    return std::make_shared<RenderPass>(render_pass, framebuffer_cache);
}

RasterPipeline CreateRasterPipeline(
    Device* device,
    const std::vector<PipelineShader<std::vector<uint8_t>>>& shaders,
    const RasterPipelineDesc& desc) {
    
    std::vector<StageDescriptorSetLayouts> stage_layouts;
    stage_layouts.reserve(shaders.size());

    for (const auto& shader : shaders) {
        rspirv_reflect::Reflection reflection;
        if (!reflection.InitFromSpirv(shader.Code.data(), shader.Code.size())) {
            throw std::runtime_error("Failed to reflect SPIR-V");
        }

        StageDescriptorSetLayouts descriptor_sets;
        if (!reflection.GetDescriptorSets(&descriptor_sets)) {
            throw std::runtime_error("Failed to get descriptor sets from reflection");
        }

        stage_layouts.push_back(std::move(descriptor_sets));
    }

    StageDescriptorSetLayouts merged_layouts = MergeShaderStageLayouts(stage_layouts);

    auto [descriptor_set_layouts, set_layout_info] = CreateDescriptorSetLayouts(
        device, &merged_layouts, VK_SHADER_STAGE_ALL_GRAPHICS, desc.DescriptorSetOpts);

    VkDevice vk_device = device->GetRawDevice();

    VkPipelineLayoutCreateInfo layout_create_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size()),
        .pSetLayouts = descriptor_set_layouts.data(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr
    };

    VkPushConstantRange push_constant_range{
        .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
        .offset = 0,
        .size = static_cast<uint32_t>(desc.PushConstantsBytes)
    };

    if (desc.PushConstantsBytes > 0) {
        layout_create_info.pushConstantRangeCount = 1;
        layout_create_info.pPushConstantRanges = &push_constant_range;
    }

    VkPipelineLayout pipeline_layout;
    VkResult result = vkCreatePipelineLayout(vk_device, &layout_create_info, nullptr, &pipeline_layout);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_create_infos;
    shader_stage_create_infos.reserve(shaders.size());
    std::vector<VkShaderModule> shader_modules;
    shader_modules.reserve(shaders.size());

    for (const auto& shader : shaders) {
        VkShaderModuleCreateInfo shader_module_info{
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .codeSize = shader.Code.size(),
            .pCode = reinterpret_cast<const uint32_t*>(shader.Code.data())
        };

        VkShaderModule shader_module;
        result = vkCreateShaderModule(vk_device, &shader_module_info, nullptr, &shader_module);
        if (result != VK_SUCCESS) {
            for (auto module : shader_modules) {
                vkDestroyShaderModule(vk_device, module, nullptr);
            }
            vkDestroyPipelineLayout(vk_device, pipeline_layout, nullptr);
            throw std::runtime_error("Failed to create shader module");
        }
        shader_modules.push_back(shader_module);

        VkShaderStageFlagBits stage;
        switch (shader.Desc.Stage) {
            case ShaderPipelineStage::Vertex:
                stage = VK_SHADER_STAGE_VERTEX_BIT;
                break;
            case ShaderPipelineStage::Pixel:
                stage = VK_SHADER_STAGE_FRAGMENT_BIT;
                break;
            default:
                for (auto module : shader_modules) {
                    vkDestroyShaderModule(vk_device, module, nullptr);
                }
                vkDestroyPipelineLayout(vk_device, pipeline_layout, nullptr);
                throw std::runtime_error("Unimplemented shader stage");
        }

        shader_stage_create_infos.push_back(VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stage,
            .module = shader_module,
            .pName = shader.Desc.Entry.c_str(),
            .pSpecializationInfo = nullptr
        });
    }

    VkPipelineVertexInputStateCreateInfo vertex_input_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr
    };

    VkPipelineInputAssemblyStateCreateInfo vertex_input_assembly_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineViewportStateCreateInfo viewport_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr
    };

    VkPipelineRasterizationStateCreateInfo rasterization_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = desc.FaceCull ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisample_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkStencilOpState noop_stencil_state{
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_KEEP,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0,
        .writeMask = 0,
        .reference = 0
    };

    VkPipelineDepthStencilStateCreateInfo depth_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = desc.DepthWrite ? VK_TRUE : VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = noop_stencil_state,
        .back = noop_stencil_state,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    size_t color_attachment_count = desc.RenderPass->FramebufferCache.ColorAttachmentCount;

    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states(
        color_attachment_count,
        VkPipelineColorBlendAttachmentState{
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_COLOR,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                             VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        }
    );

    VkPipelineColorBlendStateCreateInfo color_blend_state{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_NO_OP,
        .attachmentCount = static_cast<uint32_t>(color_blend_attachment_states.size()),
        .pAttachments = color_blend_attachment_states.data(),
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f}
    };

    std::array<VkDynamicState, 2> dynamic_state = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<uint32_t>(dynamic_state.size()),
        .pDynamicStates = dynamic_state.data()
    };

    VkGraphicsPipelineCreateInfo graphic_pipeline_info{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(shader_stage_create_infos.size()),
        .pStages = shader_stage_create_infos.data(),
        .pVertexInputState = &vertex_input_state_info,
        .pInputAssemblyState = &vertex_input_assembly_state_info,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterization_info,
        .pMultisampleState = &multisample_state_info,
        .pDepthStencilState = &depth_state_info,
        .pColorBlendState = &color_blend_state,
        .pDynamicState = &dynamic_state_info,
        .layout = pipeline_layout,
        .renderPass = desc.RenderPass->Raw,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline;
    result = vkCreateGraphicsPipelines(
        vk_device,
        VK_NULL_HANDLE,
        1,
        &graphic_pipeline_info,
        nullptr,
        &pipeline
    );

    if (result != VK_SUCCESS) {
        for (auto module : shader_modules) {
            vkDestroyShaderModule(vk_device, module, nullptr);
        }
        vkDestroyPipelineLayout(vk_device, pipeline_layout, nullptr);
        throw std::runtime_error("Unable to create graphics pipeline");
    }

    // Clean up shader modules
    for (auto module : shader_modules) {
        vkDestroyShaderModule(vk_device, module, nullptr);
    }

    std::vector<VkDescriptorPoolSize> descriptor_pool_sizes;
    for (const auto& bindings : set_layout_info) {
        for (const auto& [_, type] : bindings) {
            auto it = std::find_if(descriptor_pool_sizes.begin(), descriptor_pool_sizes.end(),
                [type](const VkDescriptorPoolSize& dps) { return dps.type == type; });

            if (it != descriptor_pool_sizes.end()) {
                it->descriptorCount += 1;
            } else {
                descriptor_pool_sizes.push_back(VkDescriptorPoolSize{
                    .type = type,
                    .descriptorCount = 1
                });
            }
        }
    }

    return RasterPipeline{
        .Common = ShaderPipelineCommon{
            .PipelineLayout = pipeline_layout,
            .Pipeline = pipeline,
            .SetLayoutInfo = set_layout_info,
            .DescriptorPoolSizes = descriptor_pool_sizes,
            .DescriptorSetLayouts = descriptor_set_layouts,
            .PipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS
        }
    };
}

} // namespace tekki::backend::vulkan