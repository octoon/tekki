#pragma once

#include <spirv/unified1/spirv.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <variant>
#include <system_error>

namespace rspirv_reflect {

// These are bit-exact with ash and the Vulkan specification
enum class DescriptorType : uint32_t {
    SAMPLER = 0,
    COMBINED_IMAGE_SAMPLER = 1,
    SAMPLED_IMAGE = 2,
    STORAGE_IMAGE = 3,
    UNIFORM_TEXEL_BUFFER = 4,
    STORAGE_TEXEL_BUFFER = 5,
    UNIFORM_BUFFER = 6,
    STORAGE_BUFFER = 7,
    UNIFORM_BUFFER_DYNAMIC = 8,
    STORAGE_BUFFER_DYNAMIC = 9,
    INPUT_ATTACHMENT = 10,
    INLINE_UNIFORM_BLOCK_EXT = 1'000'138'000,
    ACCELERATION_STRUCTURE_KHR = 1'000'150'000,
    ACCELERATION_STRUCTURE_NV = 1'000'165'000,
};

const char* ToString(DescriptorType type);

// Binding count types
enum class BindingCountType {
    One,           // Single resource binding
    StaticSized,   // Predetermined number of bindings (array)
    Unbounded      // Variable number of bindings (bindless)
};

struct BindingCount {
    BindingCountType type;
    size_t count; // Used for StaticSized

    static BindingCount One() {
        return {BindingCountType::One, 1};
    }

    static BindingCount StaticSized(size_t size) {
        return {BindingCountType::StaticSized, size};
    }

    static BindingCount Unbounded() {
        return {BindingCountType::Unbounded, 0};
    }

    bool operator==(const BindingCount& other) const {
        return type == other.type && (type != BindingCountType::StaticSized || count == other.count);
    }
};

struct DescriptorInfo {
    DescriptorType ty;
    BindingCount binding_count;
    std::string name;

    bool operator==(const DescriptorInfo& other) const {
        return ty == other.ty && binding_count == other.binding_count && name == other.name;
    }
};

struct PushConstantInfo {
    uint32_t offset;
    uint32_t size;
};

// Error types
enum class ReflectErrorCode {
    MissingBindingDecoration,
    MissingSetDecoration,
    OperandError,
    OperandIndexError,
    VariableWithoutReturnType,
    UnknownStorageClass,
    UnknownStruct,
    ImageSampledFieldUnknown,
    UnhandledTypeInstruction,
    MissingResultId,
    UnassignedResultId,
    MissingHeader,
    BindingGlobalParameterBuffer,
    TooManyPushConstants,
    ParseError,
    UnexpectedIntWidth,
    TryFromIntError,
};

class ReflectError : public std::runtime_error {
public:
    ReflectErrorCode code;

    ReflectError(ReflectErrorCode c, const std::string& msg)
        : std::runtime_error(msg), code(c) {}

    static ReflectError MissingBindingDecoration(const std::string& detail) {
        return ReflectError(ReflectErrorCode::MissingBindingDecoration,
                           "Missing binding decoration: " + detail);
    }

    static ReflectError MissingSetDecoration(const std::string& detail) {
        return ReflectError(ReflectErrorCode::MissingSetDecoration,
                           "Missing set decoration: " + detail);
    }

    static ReflectError OperandError(const std::string& detail) {
        return ReflectError(ReflectErrorCode::OperandError, "Operand error: " + detail);
    }

    static ReflectError VariableWithoutReturnType(const std::string& detail) {
        return ReflectError(ReflectErrorCode::VariableWithoutReturnType,
                           "Variable lacks return type: " + detail);
    }

    static ReflectError UnknownStorageClass(spv::StorageClass storage_class) {
        return ReflectError(ReflectErrorCode::UnknownStorageClass,
                           "Unknown storage class: " + std::to_string(static_cast<int>(storage_class)));
    }

    static ReflectError UnknownStruct(const std::string& detail) {
        return ReflectError(ReflectErrorCode::UnknownStruct, "Unknown struct: " + detail);
    }

    static ReflectError ImageSampledFieldUnknown(uint32_t value) {
        return ReflectError(ReflectErrorCode::ImageSampledFieldUnknown,
                           "Unknown sampled field value: " + std::to_string(value));
    }

    static ReflectError UnhandledTypeInstruction(const std::string& detail) {
        return ReflectError(ReflectErrorCode::UnhandledTypeInstruction,
                           "Unhandled type instruction: " + detail);
    }

    static ReflectError MissingResultId() {
        return ReflectError(ReflectErrorCode::MissingResultId, "Missing result ID");
    }

    static ReflectError UnassignedResultId(uint32_t id) {
        return ReflectError(ReflectErrorCode::UnassignedResultId,
                           "No instruction assigns to ID: " + std::to_string(id));
    }

    static ReflectError MissingHeader() {
        return ReflectError(ReflectErrorCode::MissingHeader, "Module lacks header");
    }

    static ReflectError BindingGlobalParameterBuffer() {
        return ReflectError(ReflectErrorCode::BindingGlobalParameterBuffer,
                           "Accidentally binding global parameter buffer");
    }

    static ReflectError TooManyPushConstants() {
        return ReflectError(ReflectErrorCode::TooManyPushConstants,
                           "Only one push constant block per shader entry");
    }

    static ReflectError ParseError(const std::string& detail) {
        return ReflectError(ReflectErrorCode::ParseError, "SPIR-V parse error: " + detail);
    }

    static ReflectError UnexpectedIntWidth(uint32_t width) {
        return ReflectError(ReflectErrorCode::UnexpectedIntWidth,
                           "OpTypeInt cannot have width: " + std::to_string(width));
    }
};

template<typename T>
using Result = std::variant<T, ReflectError>;

template<typename T>
bool IsOk(const Result<T>& result) {
    return std::holds_alternative<T>(result);
}

template<typename T>
bool IsErr(const Result<T>& result) {
    return std::holds_alternative<ReflectError>(result);
}

template<typename T>
T& Unwrap(Result<T>& result) {
    if (IsErr(result)) {
        throw std::get<ReflectError>(result);
    }
    return std::get<T>(result);
}

template<typename T>
const T& Unwrap(const Result<T>& result) {
    if (IsErr(result)) {
        throw std::get<ReflectError>(result);
    }
    return std::get<T>(result);
}

template<typename T>
ReflectError& UnwrapErr(Result<T>& result) {
    return std::get<ReflectError>(result);
}

// Main reflection class
class Reflection {
public:
    Reflection(std::unique_ptr<spirv_cross::Compiler> compiler);

    static Result<Reflection> NewFromSpirv(const uint32_t* spirv, size_t word_count);
    static Result<Reflection> NewFromSpirv(const std::vector<uint32_t>& spirv);

    // Get compute shader workgroup size
    std::optional<std::tuple<uint32_t, uint32_t, uint32_t>> GetComputeGroupSize() const;

    // Get descriptor sets mapping: set_index -> (binding_index -> DescriptorInfo)
    Result<std::map<uint32_t, std::map<uint32_t, DescriptorInfo>>> GetDescriptorSets();

    // Get push constant range information
    Result<std::optional<PushConstantInfo>> GetPushConstantRange();

    // Disassemble SPIR-V module
    std::string Disassemble() const;

private:
    std::unique_ptr<spirv_cross::Compiler> compiler_;
    std::vector<uint32_t> spirv_code_;

    Result<DescriptorInfo> GetDescriptorTypeForVar(
        spirv_cross::TypeID type_id,
        spv::StorageClass storage_class);

    Result<DescriptorInfo> GetDescriptorType(
        const spirv_cross::SPIRType& type,
        spv::StorageClass storage_class);

    static uint32_t ByteOffsetToLastVar(
        const spirv_cross::Compiler& compiler,
        const spirv_cross::SPIRType& struct_type);

    static uint32_t CalculateVariableSizeBytes(
        const spirv_cross::Compiler& compiler,
        const spirv_cross::SPIRType& type);
};

} // namespace rspirv_reflect
