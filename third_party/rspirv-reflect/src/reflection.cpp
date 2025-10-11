#include "rspirv-reflect/reflection.h"
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>
#include <spirv_cross/spirv_hlsl.hpp>
#include <spirv_cross/spirv_msl.hpp>
#include <spirv_cross/spirv_parser.hpp>
#include <spirv_cross/spirv_cfg.hpp>
#include <cassert>
#include <sstream>

namespace rspirv_reflect {

const char* ToString(DescriptorType type) {
    switch (type) {
        case DescriptorType::SAMPLER: return "SAMPLER";
        case DescriptorType::COMBINED_IMAGE_SAMPLER: return "COMBINED_IMAGE_SAMPLER";
        case DescriptorType::SAMPLED_IMAGE: return "SAMPLED_IMAGE";
        case DescriptorType::STORAGE_IMAGE: return "STORAGE_IMAGE";
        case DescriptorType::UNIFORM_TEXEL_BUFFER: return "UNIFORM_TEXEL_BUFFER";
        case DescriptorType::STORAGE_TEXEL_BUFFER: return "STORAGE_TEXEL_BUFFER";
        case DescriptorType::UNIFORM_BUFFER: return "UNIFORM_BUFFER";
        case DescriptorType::STORAGE_BUFFER: return "STORAGE_BUFFER";
        case DescriptorType::UNIFORM_BUFFER_DYNAMIC: return "UNIFORM_BUFFER_DYNAMIC";
        case DescriptorType::STORAGE_BUFFER_DYNAMIC: return "STORAGE_BUFFER_DYNAMIC";
        case DescriptorType::INPUT_ATTACHMENT: return "INPUT_ATTACHMENT";
        case DescriptorType::INLINE_UNIFORM_BLOCK_EXT: return "INLINE_UNIFORM_BLOCK_EXT";
        case DescriptorType::ACCELERATION_STRUCTURE_KHR: return "ACCELERATION_STRUCTURE_KHR";
        case DescriptorType::ACCELERATION_STRUCTURE_NV: return "ACCELERATION_STRUCTURE_NV";
        default: return "(UNDEFINED)";
    }
}

Reflection::Reflection(std::unique_ptr<spirv_cross::Compiler> compiler)
    : compiler_(std::move(compiler)) {
}

Result<Reflection> Reflection::NewFromSpirv(const uint32_t* spirv, size_t word_count) {
    try {
        auto compiler = std::make_unique<spirv_cross::Compiler>(spirv, word_count);
        return Reflection(std::move(compiler));
    } catch (const std::exception& e) {
        return ReflectError::ParseError(e.what());
    }
}

Result<Reflection> Reflection::NewFromSpirv(const std::vector<uint32_t>& spirv) {
    return NewFromSpirv(spirv.data(), spirv.size());
}

std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> Reflection::GetComputeGroupSize() const {
    auto& compiler = *compiler_;

    try {
        auto entry_points = compiler.get_entry_points_and_stages();
        if (entry_points.empty()) {
            return std::nullopt;
        }

        auto& entry = entry_points[0];
        if (entry.execution_model != spv::ExecutionModelGLCompute) {
            return std::nullopt;
        }

        // Get workgroup size from execution mode
        uint32_t x = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0);
        uint32_t y = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1);
        uint32_t z = compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2);

        if (x > 0 && y > 0 && z > 0) {
            return std::make_tuple(x, y, z);
        }
    } catch (const std::exception&) {
        // Ignore errors and return nullopt
    }

    return std::nullopt;
}

Result<std::map<uint32_t, std::map<uint32_t, DescriptorInfo>>> Reflection::GetDescriptorSets() {
    auto& compiler = *compiler_;
    std::map<uint32_t, std::map<uint32_t, DescriptorInfo>> unique_sets;

    try {
        auto resources = compiler.get_shader_resources();

        // Process uniform buffers
        for (auto& resource : resources.uniform_buffers) {
            auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            auto& type = compiler.get_type(resource.base_type_id);

            auto descriptor_info = GetDescriptorType(type, spv::StorageClassUniform);
            if (IsErr(descriptor_info)) {
                return UnwrapErr(descriptor_info);
            }

            auto& info = Unwrap(descriptor_info);
            info.name = resource.name;

            // Check for global parameter buffer
            if (info.name == "$Globals") {
                return ReflectError::BindingGlobalParameterBuffer();
            }

            auto& current_set = unique_sets[set];
            auto inserted = current_set.emplace(binding, info);
            if (!inserted.second) {
                // Binding already exists in this set
                return ReflectError::OperandError("Duplicate binding in set");
            }
        }

        // Process storage buffers
        for (auto& resource : resources.storage_buffers) {
            auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            auto& type = compiler.get_type(resource.base_type_id);

            auto descriptor_info = GetDescriptorType(type, spv::StorageClassStorageBuffer);
            if (IsErr(descriptor_info)) {
                return UnwrapErr(descriptor_info);
            }

            auto& info = Unwrap(descriptor_info);
            info.name = resource.name;

            auto& current_set = unique_sets[set];
            auto inserted = current_set.emplace(binding, info);
            if (!inserted.second) {
                return ReflectError::OperandError("Duplicate binding in set");
            }
        }

        // Process sampled images
        for (auto& resource : resources.sampled_images) {
            auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            auto& type = compiler.get_type(resource.base_type_id);

            auto descriptor_info = GetDescriptorType(type, spv::StorageClassUniformConstant);
            if (IsErr(descriptor_info)) {
                return UnwrapErr(descriptor_info);
            }

            auto& info = Unwrap(descriptor_info);
            info.name = resource.name;

            auto& current_set = unique_sets[set];
            auto inserted = current_set.emplace(binding, info);
            if (!inserted.second) {
                return ReflectError::OperandError("Duplicate binding in set");
            }
        }

        // Process storage images
        for (auto& resource : resources.storage_images) {
            auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            auto& type = compiler.get_type(resource.base_type_id);

            auto descriptor_info = GetDescriptorType(type, spv::StorageClassUniformConstant);
            if (IsErr(descriptor_info)) {
                return UnwrapErr(descriptor_info);
            }

            auto& info = Unwrap(descriptor_info);
            info.name = resource.name;

            auto& current_set = unique_sets[set];
            auto inserted = current_set.emplace(binding, info);
            if (!inserted.second) {
                return ReflectError::OperandError("Duplicate binding in set");
            }
        }

        // Process separate samplers
        for (auto& resource : resources.separate_samplers) {
            auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

            DescriptorInfo info{
                DescriptorType::SAMPLER,
                BindingCount::One(),
                resource.name
            };

            auto& current_set = unique_sets[set];
            auto inserted = current_set.emplace(binding, info);
            if (!inserted.second) {
                return ReflectError::OperandError("Duplicate binding in set");
            }
        }

        // Process separate images
        for (auto& resource : resources.separate_images) {
            auto set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            auto binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            auto& type = compiler.get_type(resource.base_type_id);

            auto descriptor_info = GetDescriptorType(type, spv::StorageClassUniformConstant);
            if (IsErr(descriptor_info)) {
                return UnwrapErr(descriptor_info);
            }

            auto& info = Unwrap(descriptor_info);
            info.name = resource.name;

            auto& current_set = unique_sets[set];
            auto inserted = current_set.emplace(binding, info);
            if (!inserted.second) {
                return ReflectError::OperandError("Duplicate binding in set");
            }
        }

    } catch (const std::exception& e) {
        return ReflectError::ParseError(e.what());
    }

    return unique_sets;
}

Result<DescriptorInfo> Reflection::GetDescriptorTypeForVar(
    spirv_cross::TypeID type_id,
    spv::StorageClass storage_class) {
    auto& compiler = *compiler_;
    auto& type = compiler.get_type(type_id);
    return GetDescriptorType(type, storage_class);
}

Result<DescriptorInfo> Reflection::GetDescriptorType(
    const spirv_cross::SPIRType& type,
    spv::StorageClass storage_class) {

    // Handle arrays
    if (!type.array.empty()) {
        if (type.array.size() > 1) {
            return ReflectError::UnhandledTypeInstruction("Multi-dimensional arrays not supported");
        }

        auto element_type = type;
        element_type.array.clear();

        auto element_descriptor = GetDescriptorType(element_type, storage_class);
        if (IsErr(element_descriptor)) {
            return element_descriptor;
        }

        auto& info = Unwrap(element_descriptor);

        if (type.array_size_literal[0]) {
            // Static array
            info.binding_count = BindingCount::StaticSized(type.array[0]);
        } else {
            // Runtime array (bindless)
            info.binding_count = BindingCount::Unbounded();
        }

        return info;
    }

    // Handle pointer types
    if (type.pointer) {
        auto& pointee_type = compiler_->get_type(type.self);
        return GetDescriptorType(pointee_type, storage_class);
    }

    // Determine descriptor type based on SPIRType
    DescriptorType descriptor_type = DescriptorType::UNIFORM_BUFFER;

    switch (type.basetype) {
        case spirv_cross::SPIRType::Sampler:
            descriptor_type = DescriptorType::SAMPLER;
            break;

        case spirv_cross::SPIRType::SampledImage:
            descriptor_type = DescriptorType::COMBINED_IMAGE_SAMPLER;
            break;

        case spirv_cross::SPIRType::Image: {
            auto& image = type.image;
            if (image.dim == spv::DimBuffer) {
                if (image.sampled == 1) {
                    descriptor_type = DescriptorType::UNIFORM_TEXEL_BUFFER;
                } else if (image.sampled == 2) {
                    descriptor_type = DescriptorType::STORAGE_TEXEL_BUFFER;
                } else {
                    return ReflectError::ImageSampledFieldUnknown(image.sampled);
                }
            } else if (image.dim == spv::DimSubpassData) {
                descriptor_type = DescriptorType::INPUT_ATTACHMENT;
            } else if (image.sampled == 1) {
                descriptor_type = DescriptorType::SAMPLED_IMAGE;
            } else if (image.sampled == 2) {
                descriptor_type = DescriptorType::STORAGE_IMAGE;
            } else {
                return ReflectError::ImageSampledFieldUnknown(image.sampled);
            }
            break;
        }

        case spirv_cross::SPIRType::Struct: {
            // Check storage class for struct types
            switch (storage_class) {
                case spv::StorageClassUniform:
                case spv::StorageClassUniformConstant:
                    descriptor_type = DescriptorType::UNIFORM_BUFFER;
                    break;
                case spv::StorageClassStorageBuffer:
                    descriptor_type = DescriptorType::STORAGE_BUFFER;
                    break;
                default:
                    return ReflectError::UnknownStorageClass(storage_class);
            }
            break;
        }

        case spirv_cross::SPIRType::AccelerationStructure:
            descriptor_type = DescriptorType::ACCELERATION_STRUCTURE_KHR;
            break;

        default:
            return ReflectError::UnhandledTypeInstruction("Unsupported type basetype");
    }

    return DescriptorInfo{
        descriptor_type,
        BindingCount::One(),
        ""
    };
}

Result<std::optional<PushConstantInfo>> Reflection::GetPushConstantRange() {
    auto& compiler = *compiler_;

    try {
        auto resources = compiler.get_shader_resources();

        if (resources.push_constant_buffers.empty()) {
            return std::nullopt;
        }

        if (resources.push_constant_buffers.size() > 1) {
            return ReflectError::TooManyPushConstants();
        }

        auto& push_constant = resources.push_constant_buffers[0];
        auto& type = compiler.get_type(push_constant.base_type_id);

        // Calculate size
        uint32_t size_bytes = CalculateVariableSizeBytes(compiler, type);

        return PushConstantInfo{
            .offset = 0,
            .size = size_bytes
        };

    } catch (const std::exception& e) {
        return ReflectError::ParseError(e.what());
    }
}

uint32_t Reflection::ByteOffsetToLastVar(
    const spirv_cross::Compiler& compiler,
    const spirv_cross::SPIRType& struct_type) {

    if (struct_type.member_types.empty()) {
        return 0;
    }

    uint32_t max_offset = 0;
    for (size_t i = 0; i < struct_type.member_types.size(); ++i) {
        // Get member offset
        auto offset = compiler.type_struct_member_offset(struct_type, static_cast<uint32_t>(i));
        max_offset = std::max(max_offset, offset);
    }

    return max_offset;
}

uint32_t Reflection::CalculateVariableSizeBytes(
    const spirv_cross::Compiler& compiler,
    const spirv_cross::SPIRType& type) {

    // Handle arrays first
    if (!type.array.empty()) {
        auto element_type = type;
        element_type.array.clear();
        uint32_t element_size = CalculateVariableSizeBytes(compiler, element_type);
        return element_size * type.array[0];
    }

    // Handle vectors and matrices
    if (type.vecsize > 1 || type.columns > 1) {
        auto element_type = type;
        element_type.vecsize = 1;
        element_type.columns = 1;
        uint32_t element_size = CalculateVariableSizeBytes(compiler, element_type);
        return element_size * type.vecsize * type.columns;
    }

    // Handle basic types and structs
    switch (type.basetype) {
        case spirv_cross::SPIRType::Int:
        case spirv_cross::SPIRType::UInt:
        case spirv_cross::SPIRType::Float:
            return type.width / 8;

        case spirv_cross::SPIRType::Struct: {
            if (type.member_types.empty()) {
                return 0;
            }

            uint32_t byte_offset = ByteOffsetToLastVar(compiler, type);
            auto last_member_type_id = type.member_types.back();
            auto& last_member_type = compiler.get_type(last_member_type_id);
            uint32_t last_member_size = CalculateVariableSizeBytes(compiler, last_member_type);

            return byte_offset + last_member_size;
        }

        default:
            return 0;
    }
}

std::string Reflection::Disassemble() const {
    return compiler_->compile();
}

} // namespace rspirv_reflect