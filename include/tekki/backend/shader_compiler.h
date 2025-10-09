// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0
// Original Rust: kajiya/crates/lib/kajiya-backend/src/shader_compiler.rs

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <expected>

namespace tekki::backend
{

// Compiled shader result
struct CompiledShader
{
    std::string name;
    std::vector<uint32_t> spirv; // SPIR-V bytecode
};

// Shader compilation error
struct ShaderCompilationError
{
    std::string message;
    std::string shader_name;
    std::string source_path;
};

// Shader compiler interface
class ShaderCompiler
{
  public:
    ShaderCompiler();
    ~ShaderCompiler();

    // Compile a shader from file path
    // Supports: .hlsl (HLSL), .spv (precompiled SPIR-V)
    std::expected<CompiledShader, ShaderCompilationError> CompileShader(const std::filesystem::path& path,
                                                                         const std::string& profile);

    // Compile a ray tracing shader library
    std::expected<CompiledShader, ShaderCompilationError> CompileRayTracingShader(const std::filesystem::path& path);

    // Compile HLSL source code directly
    std::expected<CompiledShader, ShaderCompilationError> CompileHLSLSource(const std::string& name,
                                                                             const std::string& source,
                                                                             const std::string& entry_point,
                                                                             const std::string& target_profile);

    // Get compute shader local size from SPIR-V
    static std::expected<std::array<uint32_t, 3>, std::string> GetComputeShaderLocalSize(
        const std::vector<uint32_t>& spirv);

    // Set shader include directories
    void SetIncludeDirectories(const std::vector<std::filesystem::path>& directories);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // Process shader includes (simplified version of shader_prepper)
    std::expected<std::string, std::string> ProcessIncludes(const std::filesystem::path& shader_path);

    // Compile HLSL using DXC
    std::expected<std::vector<uint32_t>, std::string> CompileHLSL(const std::string& name, const std::string& source,
                                                                   const std::string& entry_point,
                                                                   const std::string& target_profile);

    // Load precompiled SPIR-V file
    std::expected<std::vector<uint32_t>, std::string> LoadSPIRV(const std::filesystem::path& path);
};

// Utility functions

// Check if DXC is available
bool IsDXCAvailable();

// Get DXC version string
std::string GetDXCVersion();

} // namespace tekki::backend
