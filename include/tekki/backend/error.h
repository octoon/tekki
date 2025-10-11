#pragma once

#include <stdexcept>
#include <string>
#include <memory>
#include <vector>
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

    static BackendError Vulkan(VkResult err) {
        std::string message = "Vulkan error: " + std::to_string(static_cast<int>(err));
        return BackendError(Type::Vulkan, message);
    }

    static BackendError ResourceAccess(const std::string& info) {
        std::string message = "Invalid resource access: " + info;
        return BackendError(Type::ResourceAccess, message);
    }

private:
    Type m_type;
    std::string m_message;
};

} // namespace tekki::backend