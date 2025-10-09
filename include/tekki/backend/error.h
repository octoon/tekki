// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

// Original Rust: kajiya/crates/lib/kajiya-backend/src/error.rs

#pragma once

#include <exception>
#include <string>
#include <vulkan/vulkan.hpp>

namespace tekki::backend
{

/**
 * @brief Base backend error class
 */
class BackendError : public std::exception
{
public:
    virtual ~BackendError() = default;
    virtual const char* what() const noexcept override = 0;
    virtual std::string GetMessage() const = 0;
};

/**
 * @brief Allocation error with context
 */
class AllocationError : public BackendError
{
public:
    AllocationError(const std::string& name, const std::string& inner) : name_(name), inner_(inner)
    {
        message_ = "Allocation failed for \"" + name_ + "\": " + inner_;
    }

    const char* what() const noexcept override { return message_.c_str(); }

    std::string GetMessage() const override { return message_; }

    const std::string& GetName() const { return name_; }
    const std::string& GetInner() const { return inner_; }

private:
    std::string name_;
    std::string inner_;
    std::string message_;
};

/**
 * @brief Vulkan API error with stack trace info
 */
class VulkanError : public BackendError
{
public:
    VulkanError(vk::Result result, const std::string& trace_info = "")
        : result_(result), trace_info_(trace_info)
    {
        message_ = "Vulkan error: " + vk::to_string(result);
        if (!trace_info_.empty())
        {
            message_ += "; " + trace_info_;
        }
    }

    const char* what() const noexcept override { return message_.c_str(); }

    std::string GetMessage() const override { return message_; }

    vk::Result GetResult() const { return result_; }
    const std::string& GetTraceInfo() const { return trace_info_; }

private:
    vk::Result result_;
    std::string trace_info_;
    std::string message_;
};

/**
 * @brief Resource access error
 */
class ResourceAccessError : public BackendError
{
public:
    explicit ResourceAccessError(const std::string& info) : info_(info)
    {
        message_ = "Invalid resource access: " + info_;
    }

    const char* what() const noexcept override { return message_.c_str(); }

    std::string GetMessage() const override { return message_; }

    const std::string& GetInfo() const { return info_; }

private:
    std::string info_;
    std::string message_;
};

/**
 * @brief Helper to throw VulkanError from vk::Result
 */
inline void ThrowIfFailed(vk::Result result, const std::string& context = "")
{
    if (result != vk::Result::eSuccess)
    {
        throw VulkanError(result, context);
    }
}

} // namespace tekki::backend
