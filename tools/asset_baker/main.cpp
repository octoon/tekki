#include <tekki/kajiya_asset_pipe/lib.h>
#include <CLI/CLI.hpp>
#include <filesystem>
#include <iostream>
#include <string>

using namespace tekki::kajiya_asset_pipe;

/**
 * Bake tool - Kanelbullar
 *
 * Processes mesh assets from GLTF format into optimized .mesh and .image cache files
 */
int main(int argc, char** argv) {
    CLI::App app{"bake - Kanelbullar"};

    std::filesystem::path scenePath;
    float scale = 1.0f;
    std::string outputName;

    // Add command line options
    app.add_option("--scene", scenePath, "Path to the scene file (GLTF)")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("--scale", scale, "Scale factor for the mesh")
        ->default_val(1.0f);

    app.add_option("-o", outputName, "Output name for the baked mesh")
        ->required();

    // Parse command line arguments
    CLI11_PARSE(app, argc, argv);

    try {
        // Initialize logging (equivalent to env_logger::init())
        // spdlog is initialized automatically in the backend

        // Process the mesh asset
        MeshAssetProcessParams params{
            scenePath,
            outputName,
            scale
        };

        MeshAssetProcessor::ProcessMeshAsset(params);

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
