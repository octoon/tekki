#pragma once

#include <cstdint>
#include <string>
#include <system_error>

namespace tekki {

// 错误类型枚举
enum class AllocationErrorCode {
    OutOfMemory,
    FailedToMap,
    NoCompatibleMemoryTypeFound,
    InvalidAllocationCreateDesc,
    InvalidAllocatorCreateDesc,
    Internal,
};

// 错误类别
class AllocationErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override;
    std::string message(int ev) const override;
};

const std::error_category& GetAllocationErrorCategory();

// 创建错误码
std::error_code MakeErrorCode(AllocationErrorCode e);

// 异常类型
class AllocationError : public std::runtime_error {
public:
    explicit AllocationError(AllocationErrorCode code);
    explicit AllocationError(AllocationErrorCode code, const std::string& message);

    AllocationErrorCode Code() const { return code_; }

private:
    AllocationErrorCode code_;
};

} // namespace tekki

// 允许 std::error_code 使用 AllocationErrorCode
namespace std {
template <>
struct is_error_code_enum<tekki::AllocationErrorCode> : true_type {};
}
