#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <cassert>
#include <memory>
#include <variant>
#include <cstdlib>

namespace tekki::rust_shader_builder {

struct RustShaderCompileResult {
    std::vector<std::pair<std::string, std::string>> EntryToShaderModule;
};

class RustShaderBuilder {
public:
    static void Build() {
        const char* manifest_dir = std::getenv("CARGO_MANIFEST_DIR");
        if (!manifest_dir) {
            throw std::runtime_error("CARGO_MANIFEST_DIR environment variable not set");
        }
        auto builder_root = std::filesystem::path(manifest_dir);
        auto compile_result = SpirvBuilder::New(builder_root / "../../lib/rust-shaders/", "spirv-unknown-vulkan1.1")
            .DenyWarnings(true)
            .Capability(Capability::StorageImageWriteWithoutFormat)
            .Capability(Capability::Int8)
            .Capability(Capability::RuntimeDescriptorArray)
            .Extension("SPV_EXT_descriptor_indexing")
            .PrintMetadata(MetadataPrintout::None)
            .Multimodule(true)
            .SpirvMetadata(SpirvMetadata::NameVariables)
            .Build();

        auto target_spv_dir = builder_root / "../../../assets/rust-shaders-compiled";
        std::filesystem::create_directories(target_spv_dir);

        // Move all the compiled shaders to the `target_spv_dir`, and create a json file
        // mapping entry points to SPIR-V modules.
        if (auto multi_module = std::get_if<ModuleResult::MultiModule>(&compile_result.Module.variant)) {
            RustShaderCompileResult res;

            for (const auto& entry_pair : multi_module->entries) {
                const auto& entry = entry_pair.first;
                const auto& src_file = entry_pair.second;
                auto file_name = src_file.filename();
                auto dst_file = target_spv_dir / file_name;

                // If the compiler detects no changes, it won't generate the output,
                // so we need to check whether the file actually exists.
                if (std::filesystem::exists(src_file)) {
                    std::filesystem::rename(src_file, dst_file);
                } else {
                    if (!std::filesystem::exists(dst_file)) {
                        throw std::runtime_error("rustc failed to generate SPIR-V module " + src_file.string() + ". Try touching the source files or running `cargo clean` on shaders.");
                    }
                }

                res.EntryToShaderModule.emplace_back(entry, file_name.string());
            }

            std::ofstream out_file(target_spv_dir / "shaders.json");
            out_file << SerializeJson(res);
        } else {
            throw std::runtime_error("Expected multi-module compilation result");
        }
    }

private:
    enum class Capability {
        StorageImageWriteWithoutFormat,
        Int8,
        RuntimeDescriptorArray
    };

    enum class MetadataPrintout {
        None
    };

    enum class SpirvMetadata {
        NameVariables
    };

    struct ModuleResult {
        struct MultiModule {
            std::vector<std::pair<std::string, std::filesystem::path>> entries;
        };
        std::variant<MultiModule> variant;
    };

    struct SpirvBuilder {
        std::filesystem::path path;
        std::string target;
        bool deny_warnings = false;
        std::vector<Capability> capabilities;
        std::string extension;
        MetadataPrintout print_metadata = MetadataPrintout::None;
        bool multimodule = false;
        SpirvMetadata spirv_metadata = SpirvMetadata::NameVariables;

        static SpirvBuilder New(const std::filesystem::path& path, const std::string& target) {
            return SpirvBuilder{path, target};
        }

        SpirvBuilder& DenyWarnings(bool value) {
            deny_warnings = value;
            return *this;
        }

        SpirvBuilder& Capability(Capability capability) {
            capabilities.push_back(capability);
            return *this;
        }

        SpirvBuilder& Extension(const std::string& ext) {
            extension = ext;
            return *this;
        }

        SpirvBuilder& PrintMetadata(MetadataPrintout metadata) {
            print_metadata = metadata;
            return *this;
        }

        SpirvBuilder& Multimodule(bool value) {
            multimodule = value;
            return *this;
        }

        SpirvBuilder& SpirvMetadata(SpirvMetadata metadata) {
            spirv_metadata = metadata;
            return *this;
        }

        struct BuildResult {
            ModuleResult Module;
        };

        BuildResult Build() {
            // This would normally call the actual SPIR-V builder
            // For now, we'll return a dummy result
            ModuleResult::MultiModule multi_module;
            return BuildResult{ModuleResult{multi_module}};
        }
    };

    static std::string SerializeJson(const RustShaderCompileResult& result) {
        // Simple JSON serialization
        std::string json = "{\"EntryToShaderModule\":[";
        bool first = true;
        for (const auto& [entry, shader] : result.EntryToShaderModule) {
            if (!first) {
                json += ",";
            }
            json += "[\"" + entry + "\",\"" + shader + "\"]";
            first = false;
        }
        json += "]}";
        return json;
    }
};

} // namespace tekki::rust_shader_builder