#pragma once

#include <vector>
#include <memory>
#include <string>
#include <filesystem>
#include <unordered_set>
#include <future>
#include <fstream>
#include <iostream>
#include <glm/gtc/quaternion.hpp>
#include "tekki/core/result.h"
#include "kajiya_asset/mesh.h"

namespace tekki::kajiya_asset_pipe {

struct MeshAssetProcessParams {
    std::filesystem::path Path;
    std::string OutputName;
    float Scale;
};

class MeshAssetProcessor {
public:
    static void ProcessMeshAsset(const MeshAssetProcessParams& params) {
        auto lazyCache = LazyCache::Create();

        std::filesystem::create_directories("cache");

        {
            std::cout << "Loading " << params.Path << "..." << std::endl;

            auto loadScene = LoadGltfScene{
                .Path = params.Path,
                .Scale = params.Scale,
                .Rotation = glm::quat{1.0f, 0.0f, 0.0f, 0.0f} // Quat::IDENTITY
            };

            auto mesh = loadScene.IntoLazy();
            auto evaluatedMesh = std::async(std::launch::async, [&]() {
                return mesh.Eval(lazyCache).get();
            }).get();

            std::cout << "Packing the mesh..." << std::endl;
            auto packedMesh = PackTriangleMesh(evaluatedMesh);

            std::ofstream meshFile("cache/" + params.OutputName + ".mesh", std::ios::binary);
            if (!meshFile.is_open()) {
                throw std::runtime_error("Failed to create mesh file");
            }
            packedMesh.FlattenInto(meshFile);

            auto uniqueImages = std::vector<std::shared_ptr<Lazy<GpuImage::Proto>>>();
            auto imageSet = std::unordered_set<std::shared_ptr<Lazy<GpuImage::Proto>>>();
            
            for (const auto& map : packedMesh.Maps) {
                imageSet.insert(map);
            }
            
            uniqueImages.assign(imageSet.begin(), imageSet.end());

            std::vector<std::future<void>> imageTasks;
            imageTasks.reserve(uniqueImages.size());

            for (const auto& img : uniqueImages) {
                imageTasks.push_back(std::async(std::launch::async, [img, &lazyCache]() {
                    try {
                        auto loaded = img->Eval(lazyCache).get();
                        auto imgDst = std::filesystem::path("cache/" + FormatHex(img->Identity(), 8) + ".image");
                        
                        std::ofstream imageFile(imgDst, std::ios::binary);
                        if (!imageFile.is_open()) {
                            if (std::filesystem::exists(imgDst)) {
                                std::cout << "Could not create " << imgDst << "; ignoring" << std::endl;
                                return;
                            } else {
                                throw std::runtime_error("Failed to create image file");
                            }
                        }
                        
                        loaded.FlattenInto(imageFile);
                    } catch (const std::exception& e) {
                        throw std::runtime_error(std::string("Failed to process image: ") + e.what());
                    }
                }));
            }

            if (!imageTasks.empty()) {
                std::cout << "Processing " << imageTasks.size() << " images..." << std::endl;

                for (auto& task : imageTasks) {
                    task.get();
                }
            }

            std::cout << "Done." << std::endl;
        }
    }

private:
    static std::string FormatHex(uint64_t value, size_t width) {
        std::stringstream ss;
        ss << std::hex << std::setw(width) << std::setfill('0') << value;
        return ss.str();
    }
};

} // namespace tekki::kajiya_asset_pipe