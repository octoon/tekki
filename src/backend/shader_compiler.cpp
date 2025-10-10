#include "tekki/backend/shader_compiler.h"
#include "tekki/backend/file.h"
#include <hassle/utils.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <fstream>
#include <sstream>

namespace tekki::backend {

CompiledShader ShaderCompiler::Compile(const CompileShader& compileInfo) {
    try {
        std::string ext = compileInfo.Path.extension().string();
        if (ext.empty()) {
            ext = "";
        }

        std::string name = compileInfo.Path.stem().string();
        if (name.empty()) {
            name = "unknown";
        }

        if (ext == ".glsl") {
            throw std::runtime_error("GLSL shader compilation not implemented");
        } else if (ext == ".spv") {
            auto spirv = LoadFile::Load(compileInfo.Path);
            return CompiledShader{name, spirv};
        } else if (ext == ".hlsl") {
            std::string file_path = compileInfo.Path.string();
            auto include_provider = std::make_shared<ShaderIncludeProvider>(nullptr);
            auto source = shader_prepper::process_file(file_path, include_provider, std::string());
            std::string target_profile = compileInfo.Profile + "_6_4";
            auto spirv = CompileGenericShaderHlslImpl(name, source, target_profile);
            return CompiledShader{name, spirv};
        } else {
            throw std::runtime_error("Unrecognized shader file extension: " + ext);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Shader compilation failed: ") + e.what());
    }
}

std::vector<uint8_t> ShaderCompiler::CompileGenericShaderHlslImpl(
    const std::string& name,
    const std::string& source,
    const std::string& target_profile) {

    auto t0 = std::chrono::steady_clock::now();

    try {
        // Determine entry point based on target profile
        std::string entry_point = "main";
        if (target_profile.find("lib_") == 0) {
            // Ray tracing library doesn't need an entry point
            entry_point = "";
        }

        // Set up DXC compilation arguments for SPIR-V generation
        std::vector<std::string> args = {
            "-spirv",                       // Generate SPIR-V
            "-fspv-target-env=vulkan1.2",   // Target Vulkan 1.2
            "-WX",                          // Warnings as errors
            "-Ges",                         // Enable strict mode
            "-HV", "2021",                  // HLSL version 2021
        };

        // Additional defines (if any)
        std::vector<std::pair<std::string, std::optional<std::string>>> defines;

        // Compile the shader using hassle
        auto result = hassle::CompileHlsl(
            name,           // Source name
            source,         // Shader source code
            entry_point,    // Entry point
            target_profile, // Target profile (e.g., "cs_6_4", "lib_6_4")
            args,           // Compiler arguments
            defines         // Preprocessor defines
        );

        // Log warnings if any
        if (result.messages.has_value() && !result.messages->empty()) {
            spdlog::warn("Shader compilation warnings for '{}': {}", name, *result.messages);
        }

        auto elapsed = std::chrono::steady_clock::now() - t0;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        spdlog::info("Compiled shader '{}' in {} ms", name, elapsed_ms);

        return result.blob;

    } catch (const hassle::OperationError& e) {
        auto elapsed = std::chrono::steady_clock::now() - t0;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        spdlog::error("Shader compilation failed for '{}' after {} ms", name, elapsed_ms);
        spdlog::error("Error message: {}", e.message);

        throw std::runtime_error("HLSL compilation failed for shader '" + name + "': " + e.message);
    } catch (const std::exception& e) {
        auto elapsed = std::chrono::steady_clock::now() - t0;
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        spdlog::error("Shader compilation failed for '{}' after {} ms: {}", name, elapsed_ms, e.what());
        throw;
    }
}

RayTracingShader RayTracingShaderCompiler::Compile(const CompileRayTracingShader& compileInfo) {
    try {
        std::string file_path = compileInfo.Path.string();
        auto include_provider = std::make_shared<ShaderIncludeProvider>(nullptr);
        auto source = shader_prepper::process_file(file_path, include_provider, std::string());

        std::string ext = compileInfo.Path.extension().string();
        if (ext.empty()) {
            ext = "";
        }

        std::string name = compileInfo.Path.stem().string();
        if (name.empty()) {
            name = "unknown";
        }

        if (ext == ".glsl") {
            throw std::runtime_error("GLSL ray tracing shader compilation not implemented");
        } else if (ext == ".hlsl") {
            std::string target_profile = "lib_6_4";
            auto spirv = ShaderCompiler::CompileGenericShaderHlslImpl(name, source, target_profile);
            return RayTracingShader{name, spirv};
        } else {
            throw std::runtime_error("Unrecognized shader file extension: " + ext);
        }
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Ray tracing shader compilation failed: ") + e.what());
    }
}

ShaderIncludeProvider::ShaderIncludeProvider(std::shared_ptr<void> context) 
    : Context(context) {
}

std::string ShaderIncludeProvider::GetInclude(const std::string& path, const std::string& parentFile) {
    std::string resolved_path;
    
    if (!path.empty() && path[0] == '/') {
        resolved_path = path;
    } else {
        auto folder = std::filesystem::path(parentFile).parent_path();
        resolved_path = (folder / path).string();
    }

    try {
        auto blob = LoadFile::Load(resolved_path);
        return std::string(reinterpret_cast<const char*>(blob.data()), blob.size());
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed loading shader include ") + path + ": " + e.what());
    }
}

glm::uvec3 GetComputeShaderLocalSizeFromSpirv(const std::vector<uint32_t>& spirv) {
    // TODO: Implement SPIR-V parsing to extract compute shader local size
    // This requires SPIRV-Reflect or similar library
    // For now, return a default work group size
    throw std::runtime_error("SPIR-V reflection not yet implemented - requires SPIRV-Reflect integration");
    return glm::uvec3(1, 1, 1);
}

} // namespace tekki::backend