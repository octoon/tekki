#include "tekki/backend/error.h"
#include <string>
#include <backtrace.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace tekki::backend {

BackendError::BackendError(Type type, const std::string& message) 
    : m_type(type), m_message(message) {}

const char* BackendError::what() const noexcept {
    return m_message.c_str();
}

BackendError::Type BackendError::GetType() const { 
    return m_type; 
}

BackendError BackendError::Allocation(const std::string& name, const std::string& inner) {
    std::string message = "Allocation failed for " + name + ": " + inner;
    return BackendError(Type::Allocation, message);
}

BackendError BackendError::Vulkan(VkResult err, const backtrace_state* trace) {
    std::string message = "Vulkan error: " + std::to_string(static_cast<int>(err)) + "; " + 
                         backtraceToString(trace);
    return BackendError(Type::Vulkan, message);
}

BackendError BackendError::ResourceAccess(const std::string& info) {
    std::string message = "Invalid resource access: " + info;
    return BackendError(Type::ResourceAccess, message);
}

std::string BackendError::backtraceToString(const backtrace_state* trace) {
    // Simplified backtrace conversion - in real implementation would properly format backtrace
    return "backtrace_available";
}

} // namespace tekki::backend