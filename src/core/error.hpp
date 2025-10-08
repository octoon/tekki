// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <stdexcept>
#include <string>

#include "core/common.hpp"

namespace tekki
{

/**
 * @brief Base exception class for tekki errors
 */
class TekkiError : public std::runtime_error
{
public:
    explicit TekkiError(const std::string& message) : std::runtime_error(message) {}
};

/**
 * @brief Vulkan-related errors
 */
class VulkanError : public TekkiError
{
public:
    explicit VulkanError(const std::string& message) : TekkiError("Vulkan error: " + message) {}
};

/**
 * @brief Shader compilation/loading errors
 */
class ShaderError : public TekkiError
{
public:
    explicit ShaderError(const std::string& message) : TekkiError("Shader error: " + message) {}
};

/**
 * @brief Asset loading errors
 */
class AssetError : public TekkiError
{
public:
    explicit AssetError(const std::string& message) : TekkiError("Asset error: " + message) {}
};

/**
 * @brief Render graph errors
 */
class RenderGraphError : public TekkiError
{
public:
    explicit RenderGraphError(const std::string& message) : TekkiError("Render graph error: " + message) {}
};

} // namespace tekki
