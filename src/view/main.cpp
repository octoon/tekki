#include "tekki/view/main.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "tekki/core/Result.h"
#include "kajiya_simple.h"
#include "Opt.h"
#include "PersistedState.h"
#include "RuntimeState.h"

namespace tekki::view {

/**
 * Constructs a new AppState with persisted state and options
 */
AppState AppState::New(PersistedState persisted, const Opt& opt) {
    auto builder = SimpleMainLoop::Builder()
        .Resolution({opt.width, opt.height})
        .Vsync(!opt.no_vsync)
        .GraphicsDebugging(opt.graphics_debugging)
        .PhysicalDeviceIndex(opt.physical_device_index)
        .TemporalUpsampling(opt.temporal_upsampling)
        .DefaultLogLevel(log::LevelFilter::Info);
        
    if (opt.fullscreen) {
        builder = builder.Fullscreen(FullscreenMode::Exclusive);
    }
    
    auto kajiya = builder.Build(
        WindowBuilder::New()
            .WithTitle("kajiya")
            .WithResizable(false)
            .WithDecorations(!opt.no_window_decorations)
    );
    
    if (!kajiya) {
        throw std::runtime_error("Failed to create SimpleMainLoop");
    }
    
    auto runtime = RuntimeState::New(persisted, kajiya->GetWorldRenderer(), opt);
    
    return AppState{
        std::move(persisted),
        std::move(runtime),
        std::move(kajiya)
    };
}

/**
 * Loads a scene from the specified path
 */
void AppState::LoadScene(const std::filesystem::path& scene_path) {
    try {
        runtime_.LoadScene(persisted_, kajiya_.GetWorldRenderer(), scene_path);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to load scene: ") + e.what());
    }
}

/**
 * Adds a standalone mesh to the scene
 */
void AppState::AddStandaloneMesh(const std::filesystem::path& path, float mesh_scale) {
    try {
        runtime_.AddMeshInstance(
            persisted_,
            kajiya_.GetWorldRenderer(),
            MeshSource::File(path),
            SceneElementTransform{
                .position = glm::vec3(0.0f),
                .rotation_euler_degrees = glm::vec3(0.0f),
                .scale = glm::vec3(mesh_scale)
            }
        );
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to add mesh: ") + e.what());
    }
}

/**
 * Runs the main application loop
 */
PersistedState AppState::Run() {
    try {
        kajiya_.Run([this](auto& ctx) {
            runtime_.Frame(ctx, persisted_);
        });
        return persisted_;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Runtime error: ") + e.what());
    }
}

/**
 * Main application entry point
 */
void Main() {
    try {
        // Set VFS mount point
        SetVfsMountPoint("/meshes", "assets/meshes");

        auto opt = Opt::FromArgs();

        PersistedState persisted;
        constexpr const char* APP_STATE_CONFIG_FILE_PATH = "view_state.ron";
        
        try {
            std::ifstream file(APP_STATE_CONFIG_FILE_PATH);
            if (file.is_open()) {
                // Deserialize persisted state from RON file
                // Implementation depends on RON serialization library
                // persisted = ron::deserialize(file);
            }
        } catch (const std::exception&) {
            // Use default state if file cannot be loaded
            persisted = PersistedState();
        }

        // If supplying a new scene, clear the previous one.
        if (opt.scene.has_value() || opt.mesh.has_value()) {
            persisted.scene = SceneState();
        }

        auto state = AppState::New(std::move(persisted), opt);

        if (opt.scene.has_value()) {
            state.LoadScene(opt.scene.value());
        } else if (opt.mesh.has_value()) {
            state.AddStandaloneMesh(opt.mesh.value(), opt.mesh_scale);
        }

        auto final_state = state.Run();

        // Serialize final state to RON file
        std::ofstream out_file(APP_STATE_CONFIG_FILE_PATH);
        if (out_file.is_open()) {
            // ron::serialize_pretty(out_file, final_state, Default());
        }
        
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Application error: ") + e.what());
    }
}

} // namespace tekki::view