// tekki - C++ port of kajiya renderer
#include <fmt/format.h>
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "tekki/asset/asset_pipeline.h"
#include "tekki/core/log.h"
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <fstream>

namespace tekki::asset {

// AssetCache::Impl
struct AssetCache::Impl {
    std::unordered_map<uint64_t, std::shared_ptr<PackedTriMesh>> meshCache;
    std::unordered_map<uint64_t, std::shared_ptr<GpuImageProto>> imageCache;

    std::mutex mutex;

    uint64_t ComputeMeshHash(const GltfLoadParams& params) {
        std::string str = params.path.string();
        str += std::to_string(params.scale);
        str += std::to_string(params.rotation.x);
        str += std::to_string(params.rotation.y);
        str += std::to_string(params.rotation.z);
        str += std::to_string(params.rotation.w);

        uint64_t hash = 0xcbf29ce484222325;
        for (char c : str) {
            hash ^= static_cast<uint64_t>(c);
            hash *= 0x100000001b3;
        }
        return hash;
    }

    uint64_t ComputeImageHash(const ImageSource& source, const TexParams& params) {
        uint64_t hash = 0xcbf29ce484222325;

        if (source.IsFile()) {
            std::string path = source.GetFilePath().string();
            for (char c : path) {
                hash ^= static_cast<uint64_t>(c);
                hash *= 0x100000001b3;
            }
        } else {
            const auto& data = source.GetMemoryData();
            for (uint8_t byte : data) {
                hash ^= static_cast<uint64_t>(byte);
                hash *= 0x100000001b3;
            }
        }

        // Mix in params
        hash ^= static_cast<uint64_t>(params.gamma);
        hash ^= static_cast<uint64_t>(params.useMips);
        hash ^= static_cast<uint64_t>(params.compression);

        return hash;
    }
};

AssetCache::AssetCache() : impl_(std::make_unique<Impl>()) {}
AssetCache::~AssetCache() = default;

AssetCache& AssetCache::Instance() {
    static AssetCache instance;
    return instance;
}

std::future<Result<std::shared_ptr<PackedTriMesh>>> AssetCache::LoadMesh(
    const GltfLoadParams& params
) {
    return std::async(std::launch::async, [this, params]() -> Result<std::shared_ptr<PackedTriMesh>> {
        uint64_t hash = impl_->ComputeMeshHash(params);

        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            auto it = impl_->meshCache.find(hash);
            if (it != impl_->meshCache.end()) {
                TEKKI_LOG_INFO("Mesh cache hit: {}", params.path.string());
                std::shared_ptr<PackedTriMesh> meshPtr = it->second;  // Copy shared_ptr
                return Ok(std::move(meshPtr));
            }
        }

        TEKKI_LOG_INFO("Loading mesh: {}", params.path.string());

        GltfLoader loader;
        auto meshResult = loader.Load(params);
        if (!meshResult) {
            return Err<std::shared_ptr<PackedTriMesh>>(meshResult.Error());
        }

        auto packed = std::make_shared<PackedTriMesh>(PackTriangleMesh(*meshResult));

        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            impl_->meshCache[hash] = packed;
        }

        std::shared_ptr<PackedTriMesh> result = packed;
        return Ok(std::move(result));
    });
}

std::future<Result<std::shared_ptr<GpuImageProto>>> AssetCache::LoadImage(
    const ImageSource& source,
    const TexParams& params
) {
    return std::async(std::launch::async, [this, source, params]() -> Result<std::shared_ptr<GpuImageProto>> {
        uint64_t hash = impl_->ComputeImageHash(source, params);

        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            auto it = impl_->imageCache.find(hash);
            if (it != impl_->imageCache.end()) {
                TEKKI_LOG_INFO("Image cache hit");
                std::shared_ptr<GpuImageProto> imagePtr = it->second;  // Copy shared_ptr
                return Ok(std::move(imagePtr));
            }
        }

        TEKKI_LOG_INFO("Loading image");

        auto rawResult = ImageLoader::Load(source);
        if (!rawResult) {
            return Err<std::shared_ptr<GpuImageProto>>(rawResult.Error());
        }

        GpuImageCreator creator(params);
        auto gpuResult = creator.Create(*rawResult);
        if (!gpuResult) {
            return Err<std::shared_ptr<GpuImageProto>>(gpuResult.Error());
        }

        auto image = std::make_shared<GpuImageProto>(std::move(*gpuResult));

        {
            std::lock_guard<std::mutex> lock(impl_->mutex);
            impl_->imageCache[hash] = image;
        }

        std::shared_ptr<GpuImageProto> result = image;
        return Ok(std::move(result));
    });
}

void AssetCache::Clear() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->meshCache.clear();
    impl_->imageCache.clear();
}

// AssetProcessor::Impl
struct AssetProcessor::Impl {
    // Thread pool for parallel processing
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop = false;

    Impl() {
        size_t threadCount = std::thread::hardware_concurrency();
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });

                        if (stop && tasks.empty()) return;

                        task = std::move(tasks.front());
                        tasks.pop();
                    }

                    task();
                }
            });
        }
    }

    ~Impl() {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            stop = true;
        }
        condition.notify_all();

        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    void Enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.push(std::move(task));
        }
        condition.notify_one();
    }
};

AssetProcessor::AssetProcessor() : impl_(std::make_unique<Impl>()) {}
AssetProcessor::~AssetProcessor() = default;

Result<void> AssetProcessor::ProcessMeshAsset(const MeshAssetProcessParams& params) {
    TEKKI_LOG_INFO("Processing mesh asset: {}", params.path.string());

    // Create cache directory
    std::filesystem::create_directories("cache");

    // Load mesh
    GltfLoadParams loadParams;
    loadParams.path = params.path;
    loadParams.scale = params.scale;

    GltfLoader loader;
    auto meshResult = loader.Load(loadParams);
    if (!meshResult) {
        return Err<void>(meshResult.Error());
    }

    TEKKI_LOG_INFO("Packing mesh...");
    PackedTriMesh packed = PackTriangleMesh(*meshResult);

    // Serialize mesh
    std::string meshPath = fmt::format("cache/{}.mesh", params.outputName);
    std::vector<uint8_t> meshData = SerializePackedMesh(packed);

    std::ofstream meshFile(meshPath, std::ios::binary);
    if (!meshFile) {
        return Err<void>(fmt::format("Failed to create mesh file: {}", meshPath));
    }
    meshFile.write(reinterpret_cast<const char*>(meshData.data()), meshData.size());
    meshFile.close();

    // Collect unique images
    std::unordered_set<uint64_t> uniqueImages;
    std::vector<std::pair<uint64_t, MeshMaterialMap>> imagesToProcess;

    for (const auto& map : packed.maps) {
        if (map.IsImage()) {
            const auto& imgData = std::get<MeshMaterialMap::ImageData>(map.data);

            uint64_t hash = 0;
            if (imgData.source.IsFile()) {
                std::string pathStr = imgData.source.GetFilePath().string();
                for (char c : pathStr) {
                    hash ^= static_cast<uint64_t>(c);
                    hash *= 0x100000001b3;
                }
            }

            if (uniqueImages.insert(hash).second) {
                imagesToProcess.emplace_back(hash, map);
            }
        }
    }

    TEKKI_LOG_INFO("Processing {} unique images...", imagesToProcess.size());

    // Process images in parallel
    std::atomic<size_t> processedCount{0};
    std::mutex errorMutex;
    std::string lastError;

    for (const auto& [hash, map] : imagesToProcess) {
        impl_->Enqueue([&, hash, map]() {
            const auto& imgData = std::get<MeshMaterialMap::ImageData>(map.data);

            auto rawResult = ImageLoader::Load(imgData.source);
            if (!rawResult) {
                std::lock_guard<std::mutex> lock(errorMutex);
                lastError = rawResult.Error();
                return;
            }

            GpuImageCreator creator(imgData.params);
            auto gpuResult = creator.Create(*rawResult);
            if (!gpuResult) {
                std::lock_guard<std::mutex> lock(errorMutex);
                lastError = gpuResult.Error();
                return;
            }

            std::vector<uint8_t> imageData = SerializeGpuImage(*gpuResult);
            std::string imagePath = fmt::format("cache/{:08x}.image", static_cast<uint32_t>(hash));

            std::ofstream imageFile(imagePath, std::ios::binary);
            if (imageFile) {
                imageFile.write(reinterpret_cast<const char*>(imageData.data()), imageData.size());
            }

            processedCount++;
        });
    }

    // Wait for all tasks
    while (processedCount < imagesToProcess.size()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (!lastError.empty()) {
        return Err<void>(fmt::format("Image processing error: {}", lastError));
    }

    TEKKI_LOG_INFO("Done processing mesh asset");

    return Ok();
}

} // namespace tekki::asset
