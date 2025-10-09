#pragma once

#include <stdexcept>
#include <string>
#include <memory>
#include <vector>
#include <backtrace.h>
#include <vulkan/vulkan.h>
#include "tekki/core/result.h"

namespace tekki::backend {

class BackendError : public std::exception {
public:
    enum class Type {
        Allocation,
        Vulkan,
        ResourceAccess
    };

    BackendError(Type type, const std::string& message) : m_type(type), m_message(message) {}

    virtual const char* what() const noexcept override {
        return m_message.c_str();
    }

    Type GetType() const { return m_type; }

    static BackendError Allocation(const std::string& name, const std::string& inner) {
        std::string message = "Allocation failed for " + name + ": " + inner;
        return BackendError(Type::Allocation, message);
    }

    static BackendError Vulkan(VkResult err, const backtrace_state* trace) {
        std::string message = "Vulkan error: " + std::to_string(static_cast<int>(err)) + "; " + 
                             backtraceToString(trace);
        return BackendError(Type::Vulkan, message);
    }

    static BackendError ResourceAccess(const std::string& info) {
        std::string message = "Invalid resource access: " + info;
        return BackendError(Type::ResourceAccess, message);
    }

private:
    Type m_type;
    std::string m_message;

    static std::string backtraceToString(const backtrace_state* trace) {
        // Simplified backtrace conversion - in real implementation would properly format backtrace
        return "backtrace_available";
    }
};

} // namespace tekki::backend