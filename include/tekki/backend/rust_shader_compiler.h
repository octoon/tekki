#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <glm/glm.hpp>
#include "tekki/core/result.h"
#include "tekki/backend/file.h"
#include "tekki/shader_compiler/CompiledShader.h"

namespace tekki::backend {

class CompileRustShader {
public:
    CompileRustShader(const std::string& entry) : Entry(entry) {}
    
    CompiledShader Run() {
        // Compile the Rust shader crate first
        CompileRustShaderCrate().Run();
        
        // Load the compilation results
        auto compileResultData = LoadFile("/rust-shaders-compiled/shaders.json").Run();
        auto compileResult = RustShaderCompileResult::DeserializeJson(
            std::string(reinterpret_cast<const char*>(compileResultData.data()), compileResultData.size()));
        
        // Find the shader module for our entry point
        std::string shaderFile;
        bool found = false;
        for (const auto& [entry, module] : compileResult.EntryToShaderModule) {
            if (entry == Entry) {
                shaderFile = module;
                found = true;
                break;
            }
        }
        
        if (!found) {
            throw std::runtime_error("No Rust-GPU module found for entry point " + Entry);
        }
        
        // Load the actual SPIR-V blob
        auto spirvBlob = LoadFile("/rust-shaders-compiled/" + shaderFile).Run();
        
        return CompiledShader{
            "rust-gpu",
            std::vector<uint8_t>(spirvBlob.begin(), spirvBlob.end())
        };
    }

private:
    std::string Entry;
};

struct RustShaderCompileResult {
    // entry name -> shader path
    std::vector<std::pair<std::string, std::string>> EntryToShaderModule;
    
    static RustShaderCompileResult DeserializeJson(const std::string& json) {
        RustShaderCompileResult result;
        // Simple JSON parsing - in practice you'd use a proper JSON library
        // This is a simplified implementation
        std::istringstream stream(json);
        std::string line;
        bool inArray = false;
        std::string currentEntry;
        std::string currentModule;
        
        while (std::getline(stream, line)) {
            if (line.find("\"entry_to_shader_module\"") != std::string::npos) {
                inArray = true;
            } else if (inArray && line.find('[') != std::string::npos) {
                // Start of array
            } else if (inArray && line.find(']') != std::string::npos) {
                // End of array
                inArray = false;
            } else if (inArray && line.find('\"') != std::string::npos) {
                // Parse key-value pairs
                size_t quote1 = line.find('\"');
                size_t quote2 = line.find('\"', quote1 + 1);
                size_t quote3 = line.find('\"', quote2 + 1);
                size_t quote4 = line.find('\"', quote3 + 1);
                
                if (quote1 != std::string::npos && quote2 != std::string::npos && 
                    quote3 != std::string::npos && quote4 != std::string::npos) {
                    currentEntry = line.substr(quote1 + 1, quote2 - quote1 - 1);
                    currentModule = line.substr(quote3 + 1, quote4 - quote3 - 1);
                    result.EntryToShaderModule.emplace_back(currentEntry, currentModule);
                }
            }
        }
        
        return result;
    }
};

class CompileRustShaderCrate {
public:
    void Run() {
        std::vector<std::filesystem::path> srcDirs;
        try {
            srcDirs = {
                NormalizedPathFromVfs("/kajiya/crates/lib/rust-shaders/src"),
                NormalizedPathFromVfs("/kajiya/crates/lib/rust-shaders-shared/src")
            };
        } catch (const std::exception&) {
            // Log info: Rust shader sources not found. Using the precompiled versions.
            return;
        }

        // Unlike regular shader building, this one runs in a separate thread in the background.
        //
        // The built shaders are cached and checked-in, meaning that
        // 1. Devs/users don't need to have Rust-GPU
        // 2. The previously built shaders can be used at startup without stalling the app
        //
        // To accomplish such behavior, this function lies to `turbosloth`, immediately claiming success.
        // The caller then goes straight for the cached shaders. Meanwhile, a thread is spawned,
        // and builds the shaders. When that's done, `CompileRustShader` which depends on this
        // will notice a change in the compiler output files, and trigger the shader reload.

        // In case `CompileRustShaderCrate` gets cancelled, we will want to cancel
        // the builder thread as well. We'll use an atomic flag to do this.
        static std::mutex BuildTaskCancelMutex;
        static std::atomic<bool> BuildTaskCancelFlag{false};
        
        {
            std::lock_guard<std::mutex> lock(BuildTaskCancelMutex);
            BuildTaskCancelFlag.store(true, std::memory_order_release);
            BuildTaskCancelFlag.store(false, std::memory_order_release);
        }

        // Spawn the worker thread.
        std::thread workerThread([this]() {
            try {
                // Log info: Building Rust-GPU shaders in the background...
                CompileRustShaderCrateThread();
            } catch (const std::exception& err) {
                // Log error: Failed to build Rust-GPU shaders. Falling back to the previously compiled ones.
            }
        });
        workerThread.detach();

        // And finally register a watcher on the source directory for Rust shaders.
        for (const auto& srcDir : srcDirs) {
            try {
                auto invalidationTrigger = GetInvalidationTrigger();
                FileWatcher::GetInstance().Watch(srcDir, [invalidationTrigger](const FileWatcher::Event& event) {
                    if (event.Type == FileWatcher::EventType::Write) {
                        invalidationTrigger();
                    }
                });
            } catch (const std::exception& e) {
                throw std::runtime_error("CompileRustShaderCrate: trying to watch " + srcDir.string() + ": " + e.what());
            }
        }
    }

private:
    // Runs cargo in a sub-process to execute the rust shader builder.
    void CompileRustShaderCrateThread() {
        auto builderDir = NormalizedPathFromVfs("/kajiya/crates/bin/rust-shader-builder");
        
        std::string command = "cargo run --release --";
        std::string workingDir = builderDir.string();
        
        // Remove environment variables that might interfere
        auto env = std::getenv("PATH"); // Basic environment preservation
        
        // Execute the command
        std::string fullCommand = "cd \"" + workingDir + "\" && " + command;
        
        FILE* pipe = popen(fullCommand.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("failed to execute Rust-GPU builder");
        }
        
        char buffer[128];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        
        int returnCode = pclose(pipe);
        
        if (returnCode != 0) {
            throw std::runtime_error("Shader builder failed:\n" + result);
        } else {
            // Log info: Rust-GPU cargo process finished.
        }
    }
    
    std::function<void()> GetInvalidationTrigger() {
        return []() {
            // Implementation would depend on your invalidation system
        };
    }
    
    std::filesystem::path NormalizedPathFromVfs(const std::string& path) {
        // Implementation would depend on your VFS system
        return std::filesystem::path(path);
    }
};

} // namespace tekki::backend