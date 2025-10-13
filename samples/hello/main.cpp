#include <tekki/kajiya_simple/lib.h>
#include <tekki/renderer/world_renderer.h>
#include <tekki/renderer/camera.h>
#include <tekki/renderer/frame_desc.h>
#include <tekki/renderer/mmap.h>
#include <tekki/asset/mesh.h>
#include <tekki/backend/file.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <memory>
#include <stdexcept>

using namespace tekki::kajiya_simple;
using namespace tekki::renderer;
using namespace tekki::asset;
using namespace tekki::backend::file;
using namespace tekki::rust_shaders_shared;

// Helper function to convert renderer::CameraMatrices to rust_shaders_shared::CameraMatrices
CameraMatrices ToCameraMatrices(const tekki::renderer::CameraMatrices& cam) {
    CameraMatrices result;
    // Copy matrix data - assuming compatible layout
    std::memcpy(result.ViewToClip, &cam.ViewToClip[0][0], sizeof(result.ViewToClip));
    std::memcpy(result.ClipToView, &cam.ClipToView[0][0], sizeof(result.ClipToView));
    std::memcpy(result.WorldToView, &cam.WorldToView[0][0], sizeof(result.WorldToView));
    std::memcpy(result.ViewToWorld, &cam.ViewToWorld[0][0], sizeof(result.ViewToWorld));
    return result;
}

int main() {
    try {
        // Set up VFS mount points for asset loading
        SetStandardVfsMountPoints();

        // Build the simple main loop with resolution 1920x1080
        auto kajiya = SimpleMainLoop::Builder()
            .Resolution(glm::uvec2(1920, 1080))
            .Build(WindowBuilder::New()
                .WithTitle("hello-kajiya")
                .WithResizable(false));

        if (!kajiya) {
            throw std::runtime_error("Failed to create SimpleMainLoop");
        }

        // Set up camera position and rotation
        glm::vec3 cameraPosition(0.0f, 1.0f, 2.5f);
        glm::quat cameraRotation = glm::angleAxis(glm::radians(-18.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        auto camera = std::make_pair(cameraPosition, cameraRotation);

        // Set up camera lens
        tekki::renderer::CameraLens lens;
        lens.AspectRatio = kajiya->WindowAspectRatio();

        // Load the car mesh using memory-mapped asset
        auto carMeshFlat = AssetMmapManager::MmappedAsset<PackedTriMesh::Flat>("/cache/336_lrm.mesh");
        if (!carMeshFlat) {
            throw std::runtime_error("Failed to load mesh from /cache/336_lrm.mesh");
        }

        // Add mesh to the world renderer
        AddMeshOptions meshOptions;
        auto carMesh = kajiya->GetWorldRenderer().AddMesh(carMeshFlat, meshOptions);

        // Create instance of the car mesh
        glm::mat4 identityTransform = glm::mat4(1.0f);
        auto carInst = kajiya->GetWorldRenderer().AddInstance(carMesh, identityTransform);

        // Initialize car rotation
        float carRot = 0.0f;

        // Run the main loop
        kajiya->Run([&](FrameContext& ctx) {
            // Update car rotation
            carRot += 0.5f * ctx.DtFiltered;

            // Update instance transform with rotation
            glm::quat rotation = glm::angleAxis(carRot, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 transform = glm::mat4_cast(rotation);
            ctx.WorldRenderer.SetInstanceTransform(carInst, transform);

            // Calculate camera matrices
            auto cameraBodyMatrices = tekki::renderer::CameraBodyMatrices::FromPositionRotation(
                camera.first, camera.second
            );
            auto lensMatrices = lens.CalcMatrices();

            tekki::renderer::CameraMatrices cameraMatrices{
                lensMatrices.ViewToClip,
                lensMatrices.ClipToView,
                cameraBodyMatrices.WorldToView,
                cameraBodyMatrices.ViewToWorld
            };

            // Convert to rust shaders shared camera matrices
            auto shaderCameraMatrices = ToCameraMatrices(cameraMatrices);

            // Return frame description
            return WorldFrameDesc{
                shaderCameraMatrices,
                glm::vec2(ctx.RenderExtent),
                glm::normalize(glm::vec3(4.0f, 1.0f, 1.0f))  // Sun direction
            };
        });

        return 0;

    } catch (const std::exception& e) {
        fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}

