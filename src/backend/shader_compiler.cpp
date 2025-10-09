// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0
// Original Rust: kajiya/crates/lib/kajiya-backend/src/shader_compiler.rs

#include "../../include/tekki/backend/shader_compiler.h"
#include <cstdlib>
#include <fstream>
#include <regex>
#include <spdlog/spdlog.h>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace tekki::backend
{

// Implementation details
struct ShaderCompiler::Impl
{
    std::vector<std::filesystem::path> include_directories;
    std::filesystem::path dxc_path;

    Impl()
    {
        // Try to find DXC in common locations
        FindDXC();
    }

    void FindDXC()
    {
#ifdef _WIN32
        // Check environment variable
        if (const char* dxc_env = std::getenv("DXC_PATH"))
        {
            dxc_path = dxc_env;
            if (std::filesystem::exists(dxc_path))
            {
                spdlog::info("Found DXC at: {}", dxc_path.string());
                return;
            }
        }

        // Check common installation paths
        std::vector<std::filesystem::path> search_paths = {
            "C:/Program Files (x86)/Windows Kits/10/bin/x64/dxc.exe",
            "C:/Program Files/Windows Kits/10/bin/x64/dxc.exe",
            "dxc.exe", // Check PATH
        };

        for (const auto& path : search_paths)
        {
            if (std::filesystem::exists(path))
            {
                dxc_path = path;
                spdlog::info("Found DXC at: {}", dxc_path.string());
                return;
            }
        }

        // Try to find in PATH
        dxc_path = "dxc.exe";
        spdlog::warn("DXC not found in common locations, will try PATH");
#else
        // Linux/macOS
        dxc_path = "dxc";
        spdlog::info("Using DXC from PATH");
#endif
    }
};

ShaderCompiler::ShaderCompiler() : impl_(std::make_unique<Impl>()) {}

ShaderCompiler::~ShaderCompiler() = default;

void ShaderCompiler::SetIncludeDirectories(const std::vector<std::filesystem::path>& directories)
{
    impl_->include_directories = directories;
}

std::expected<CompiledShader, ShaderCompilationError> ShaderCompiler::CompileShader(const std::filesystem::path& path,
                                                                                     const std::string& profile)
{
    std::string ext = path.extension().string();
    std::string name = path.stem().string();

    if (ext == ".spv")
    {
        // Load precompiled SPIR-V
        auto spirv_result = LoadSPIRV(path);
        if (!spirv_result)
        {
            return std::unexpected(ShaderCompilationError{
                .message = spirv_result.error(), .shader_name = name, .source_path = path.string()});
        }

        return CompiledShader{.name = name, .spirv = std::move(*spirv_result)};
    }
    else if (ext == ".hlsl")
    {
        // Process includes
        auto source_result = ProcessIncludes(path);
        if (!source_result)
        {
            return std::unexpected(ShaderCompilationError{
                .message = source_result.error(), .shader_name = name, .source_path = path.string()});
        }

        // Compile HLSL
        std::string target_profile = profile + "_6_4";
        auto spirv_result = CompileHLSL(name, *source_result, "main", target_profile);

        if (!spirv_result)
        {
            return std::unexpected(ShaderCompilationError{
                .message = spirv_result.error(), .shader_name = name, .source_path = path.string()});
        }

        return CompiledShader{.name = name, .spirv = std::move(*spirv_result)};
    }
    else
    {
        return std::unexpected(ShaderCompilationError{
            .message = "Unrecognized shader file extension: " + ext, .shader_name = name, .source_path = path.string()});
    }
}

std::expected<CompiledShader, ShaderCompilationError>
ShaderCompiler::CompileRayTracingShader(const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    std::string name = path.stem().string();

    if (ext == ".hlsl")
    {
        // Process includes
        auto source_result = ProcessIncludes(path);
        if (!source_result)
        {
            return std::unexpected(ShaderCompilationError{
                .message = source_result.error(), .shader_name = name, .source_path = path.string()});
        }

        // Compile as ray tracing library
        std::string target_profile = "lib_6_4";
        auto spirv_result = CompileHLSL(name, *source_result, "main", target_profile);

        if (!spirv_result)
        {
            return std::unexpected(ShaderCompilationError{
                .message = spirv_result.error(), .shader_name = name, .source_path = path.string()});
        }

        return CompiledShader{.name = name, .spirv = std::move(*spirv_result)};
    }
    else
    {
        return std::unexpected(ShaderCompilationError{
            .message = "Unrecognized shader file extension: " + ext, .shader_name = name, .source_path = path.string()});
    }
}

std::expected<CompiledShader, ShaderCompilationError>
ShaderCompiler::CompileHLSLSource(const std::string& name, const std::string& source, const std::string& entry_point,
                                  const std::string& target_profile)
{
    auto spirv_result = CompileHLSL(name, source, entry_point, target_profile);

    if (!spirv_result)
    {
        return std::unexpected(
            ShaderCompilationError{.message = spirv_result.error(), .shader_name = name, .source_path = "<source>"});
    }

    return CompiledShader{.name = name, .spirv = std::move(*spirv_result)};
}

std::expected<std::string, std::string> ShaderCompiler::ProcessIncludes(const std::filesystem::path& shader_path)
{
    // Read shader file
    std::ifstream file(shader_path);
    if (!file)
    {
        return std::unexpected("Failed to open shader file: " + shader_path.string());
    }

    std::stringstream result;
    std::string line;
    std::regex include_regex(R"(^\s*#\s*include\s+[<"]([^>"]+)[>"])");

    while (std::getline(file, line))
    {
        std::smatch match;
        if (std::regex_search(line, match, include_regex))
        {
            std::string include_path = match[1].str();

            // Resolve include path
            std::filesystem::path resolved_path;

            if (include_path[0] == '/')
            {
                // Absolute path (relative to shader root)
                for (const auto& include_dir : impl_->include_directories)
                {
                    auto candidate = include_dir / include_path.substr(1);
                    if (std::filesystem::exists(candidate))
                    {
                        resolved_path = candidate;
                        break;
                    }
                }

                if (resolved_path.empty())
                {
                    resolved_path = include_path.substr(1);
                }
            }
            else
            {
                // Relative path
                resolved_path = shader_path.parent_path() / include_path;
            }

            // Read include file
            std::ifstream include_file(resolved_path);
            if (!include_file)
            {
                return std::unexpected("Failed to open include file: " + resolved_path.string());
            }

            std::string include_content((std::istreambuf_iterator<char>(include_file)),
                                        std::istreambuf_iterator<char>());

            result << include_content << "\n";
        }
        else
        {
            result << line << "\n";
        }
    }

    return result.str();
}

std::expected<std::vector<uint32_t>, std::string> ShaderCompiler::CompileHLSL(const std::string& name,
                                                                               const std::string& source,
                                                                               const std::string& entry_point,
                                                                               const std::string& target_profile)
{
    // Write source to temporary file
    std::filesystem::path temp_dir = std::filesystem::temp_directory_path();
    std::filesystem::path source_file = temp_dir / (name + ".hlsl");
    std::filesystem::path output_file = temp_dir / (name + ".spv");

    {
        std::ofstream out(source_file);
        if (!out)
        {
            return std::unexpected("Failed to create temporary source file");
        }
        out << source;
    }

    // Build DXC command line
    std::ostringstream cmd;
    cmd << impl_->dxc_path.string() << " ";
    cmd << "-spirv ";
    cmd << "-fspv-target-env=vulkan1.2 ";
    cmd << "-WX "; // Warnings as errors
    cmd << "-Ges "; // Strict mode
    cmd << "-HV 2021 "; // HLSL 2021
    cmd << "-T " << target_profile << " ";
    cmd << "-E " << entry_point << " ";

    // Add include directories
    for (const auto& include_dir : impl_->include_directories)
    {
        cmd << "-I \"" << include_dir.string() << "\" ";
    }

    cmd << "-Fo \"" << output_file.string() << "\" ";
    cmd << "\"" << source_file.string() << "\"";

    std::string command = cmd.str();
    spdlog::debug("Compiling shader with DXC: {}", command);

    auto t0 = std::chrono::high_resolution_clock::now();

    // Execute DXC
    int result = std::system(command.c_str());

    auto t1 = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (result != 0)
    {
        std::filesystem::remove(source_file);
        return std::unexpected("DXC compilation failed for shader: " + name);
    }

    spdlog::trace("DXC took {}ms for {}", duration, name);

    // Read compiled SPIR-V
    auto spirv_result = LoadSPIRV(output_file);

    // Cleanup temporary files
    std::filesystem::remove(source_file);
    std::filesystem::remove(output_file);

    if (!spirv_result)
    {
        return std::unexpected("Failed to load compiled SPIR-V: " + spirv_result.error());
    }

    return spirv_result;
}

std::expected<std::vector<uint32_t>, std::string> ShaderCompiler::LoadSPIRV(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        return std::unexpected("Failed to open SPIR-V file: " + path.string());
    }

    // Get file size
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size % 4 != 0)
    {
        return std::unexpected("Invalid SPIR-V file size (not multiple of 4): " + path.string());
    }

    // Read SPIR-V
    std::vector<uint32_t> spirv(size / 4);
    file.read(reinterpret_cast<char*>(spirv.data()), size);

    if (!file)
    {
        return std::unexpected("Failed to read SPIR-V file: " + path.string());
    }

    return spirv;
}

std::expected<std::array<uint32_t, 3>, std::string>
ShaderCompiler::GetComputeShaderLocalSize(const std::vector<uint32_t>& spirv)
{
    if (spirv.size() < 5)
    {
        return std::unexpected("Invalid SPIR-V: too small");
    }

    // SPIR-V magic number check
    if (spirv[0] != 0x07230203)
    {
        return std::unexpected("Invalid SPIR-V magic number");
    }

    // Parse SPIR-V to find ExecutionMode instruction
    // OpExecutionMode = 16
    // ExecutionMode LocalSize = 17

    for (size_t i = 5; i < spirv.size();)
    {
        uint32_t word_count = spirv[i] >> 16;
        uint32_t opcode = spirv[i] & 0xFFFF;

        if (word_count == 0)
        {
            break; // Invalid instruction
        }

        // OpExecutionMode
        if (opcode == 16 && word_count >= 6)
        {
            uint32_t execution_mode = spirv[i + 2];

            // LocalSize = 17
            if (execution_mode == 17)
            {
                return std::array<uint32_t, 3>{spirv[i + 3], spirv[i + 4], spirv[i + 5]};
            }
        }

        i += word_count;
    }

    return std::unexpected("Could not find ExecutionMode LocalSize in SPIR-V");
}

bool IsDXCAvailable()
{
    ShaderCompiler compiler;
    return std::filesystem::exists(compiler.impl_->dxc_path) ||
           std::system((compiler.impl_->dxc_path.string() + " --version > /dev/null 2>&1").c_str()) == 0;
}

std::string GetDXCVersion()
{
    // TODO: Parse DXC version output
    return "unknown";
}

} // namespace tekki::backend
