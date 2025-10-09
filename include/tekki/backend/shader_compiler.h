#pragma once

#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <chrono>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/file.h"

namespace tekki::backend {

struct CompiledShader {
    std::string Name;
    std::vector<uint8_t> Spirv;
};

struct CompileShader {
    std::filesystem::path Path;
    std::string Profile;
    
    bool operator==(const CompileShader& other) const = default;
};

class ShaderCompiler {
public:
    static CompiledShader Compile(const CompileShader& compileInfo);
    
private:
    static std::vector<uint8_t> CompileGenericShaderHlslImpl(
        const std::string& name, 
        const std::string& source, 
        const std::string& targetProfile);
};

struct RayTracingShader {
    std::string Name;
    std::vector<uint8_t> Spirv;
};

struct CompileRayTracingShader {
    std::filesystem::path Path;
    
    bool operator==(const CompileRayTracingShader& other) const = default;
};

class RayTracingShaderCompiler {
public:
    static RayTracingShader Compile(const CompileRayTracingShader& compileInfo);
};

class ShaderIncludeProvider {
public:
    explicit ShaderIncludeProvider(std::shared_ptr<void> context);
    
    std::string GetInclude(const std::string& path, const std::string& parentFile);

private:
    std::shared_ptr<void> Context;
};

glm::uvec3 GetComputeShaderLocalSizeFromSpirv(const std::vector<uint32_t>& spirv);

} // namespace tekki::backend