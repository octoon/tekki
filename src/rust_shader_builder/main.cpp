#include "tekki/rust_shader_builder/main.h"
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace tekki::rust_shader_builder {

void RustShaderBuilder::Build() {
    try {
        auto builder_root = std::filesystem::path(std::getenv("CARGO_MANIFEST_DIR"));
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
            
            for (const auto& [entry, src_file] : multi_module->entries) {
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
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("RustShaderBuilder::Build failed: ") + e.what());
    }
}

RustShaderBuilder::SpirvBuilder RustShaderBuilder::SpirvBuilder::New(const std::filesystem::path& path, const std::string& target) {
    return SpirvBuilder{path, target};
}

RustShaderBuilder::SpirvBuilder& RustShaderBuilder::SpirvBuilder::DenyWarnings(bool value) {
    deny_warnings = value;
    return *this;
}

RustShaderBuilder::SpirvBuilder& RustShaderBuilder::SpirvBuilder::Capability(Capability capability) {
    capabilities.push_back(capability);
    return *this;
}

RustShaderBuilder::SpirvBuilder& RustShaderBuilder::SpirvBuilder::Extension(const std::string& ext) {
    extension = ext;
    return *this;
}

RustShaderBuilder::SpirvBuilder& RustShaderBuilder::SpirvBuilder::PrintMetadata(MetadataPrintout metadata) {
    print_metadata = metadata;
    return *this;
}

RustShaderBuilder::SpirvBuilder& RustShaderBuilder::SpirvBuilder::Multimodule(bool value) {
    multimodule = value;
    return *this;
}

RustShaderBuilder::SpirvBuilder& RustShaderBuilder::SpirvBuilder::SpirvMetadata(SpirvMetadata metadata) {
    spirv_metadata = metadata;
    return *this;
}

RustShaderBuilder::SpirvBuilder::BuildResult RustShaderBuilder::SpirvBuilder::Build() {
    try {
        // This would normally call the actual SPIR-V builder
        // For now, we'll return a dummy result
        ModuleResult::MultiModule multi_module;
        return BuildResult{ModuleResult{multi_module}};
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("SpirvBuilder::Build failed: ") + e.what());
    }
}

std::string RustShaderBuilder::SerializeJson(const RustShaderCompileResult& result) {
    try {
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
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("SerializeJson failed: ") + e.what());
    }
}

} // namespace tekki::rust_shader_builder