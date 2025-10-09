#include "tekki/backend/shader_compiler.h"
#include "tekki/backend/file.h"
#include <hassel_rs.hpp>
#include <rspirv/dr/dr.h>
#include <rspirv/binary/parser.h>
#include <shader_prepper.hpp>
#include <glm/glm.hpp>
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
    
    std::vector<std::string> args = {
        "-spirv",
        "-fspv-target-env=vulkan1.2",
        "-WX",
        "-Ges",
        "-HV 2021"
    };
    
    auto spirv = hassle_rs::compile_hlsl(
        name,
        source,
        "main",
        target_profile,
        args,
        std::vector<std::string>()
    );
    
    auto elapsed = std::chrono::steady_clock::now() - t0;
    
    return spirv;
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
    rspirv::dr::Loader loader;
    rspirv::binary::parse_words(spirv.data(), spirv.size(), &loader);
    auto module = loader.module();

    for (const auto& inst : module.global_instructions()) {
        if (inst.opcode() == spv::Op::OpExecutionMode) {
            const auto& operands = inst.operands();
            if (operands.size() >= 5) {
                uint32_t x = operands[2].AsLiteralInteger();
                uint32_t y = operands[3].AsLiteralInteger();
                uint32_t z = operands[4].AsLiteralInteger();
                return glm::uvec3(x, y, z);
            } else {
                throw std::runtime_error("Could not parse the ExecutionMode SPIR-V op");
            }
        }
    }

    throw std::runtime_error("Could not find a ExecutionMode SPIR-V op");
}

} // namespace tekki::backend