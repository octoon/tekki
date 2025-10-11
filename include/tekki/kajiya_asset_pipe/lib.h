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
    static void ProcessMeshAsset(const MeshAssetProcessParams& params);

private:
    static std::string FormatHex(uint64_t value, size_t width);
};

} // namespace tekki::kajiya_asset_pipe