#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include "tekki/core/result.h"
#include "tekki/backend/file.h"
#include "tekki/backend/shader_compiler.h"

namespace tekki::backend {

class CompileRustShader {
public:
    CompileRustShader(const std::string& entry) : Entry(entry) {}

    CompiledShader Run();

private:
    std::string Entry;
};

struct RustShaderCompileResult {
    // entry name -> shader path
    std::vector<std::pair<std::string, std::string>> EntryToShaderModule;

    static RustShaderCompileResult DeserializeJson(const std::string& json);
};

class CompileRustShaderCrate {
public:
    void Run();

private:
    void CompileRustShaderCrateThread();
    std::function<void()> GetInvalidationTrigger();
    std::filesystem::path NormalizedPathFromVfs(const std::string& path);
};

} // namespace tekki::backend