#include "tekki/gpu_allocator/result.h"

namespace tekki {

const char* AllocationErrorCategory::name() const noexcept {
    return "allocation_error";
}

std::string AllocationErrorCategory::message(int ev) const {
    switch (static_cast<AllocationErrorCode>(ev)) {
        case AllocationErrorCode::OutOfMemory:
            return "Out of memory";
        case AllocationErrorCode::FailedToMap:
            return "Failed to map memory";
        case AllocationErrorCode::NoCompatibleMemoryTypeFound:
            return "No compatible memory type available";
        case AllocationErrorCode::InvalidAllocationCreateDesc:
            return "Invalid AllocationCreateDesc";
        case AllocationErrorCode::InvalidAllocatorCreateDesc:
            return "Invalid AllocatorCreateDesc";
        case AllocationErrorCode::Internal:
            return "Internal error";
        default:
            return "Unknown error";
    }
}

const std::error_category& GetAllocationErrorCategory() {
    static AllocationErrorCategory category;
    return category;
}

std::error_code MakeErrorCode(AllocationErrorCode e) {
    return {static_cast<int>(e), GetAllocationErrorCategory()};
}

AllocationError::AllocationError(AllocationErrorCode code)
    : std::runtime_error(GetAllocationErrorCategory().message(static_cast<int>(code))),
      code_(code) {}

AllocationError::AllocationError(AllocationErrorCode code, const std::string& message)
    : std::runtime_error(message),
      code_(code) {}

} // namespace tekki
