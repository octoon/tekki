#pragma once

#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include "tekki/backend/vulkan/image.h"
#include "tekki/render_graph/lib.h"
#include "tekki/backend/file.h"

namespace tekki::renderer::renderers {

struct ImageRgba16f {
    glm::uvec2 Size;
    std::vector<uint16_t> Data;

    ImageRgba16f() : Size(0, 0), Data() {}
    
    ImageRgba16f(uint32_t width, uint32_t height) : Size(width, height), Data((width * height * 4), 0) {}

    void PutPixel(uint32_t x, uint32_t y, const std::array<uint16_t, 4>& rgba) {
        size_t offset = ((y * Size.x + x) * 4);
        if (offset + 3 < Data.size()) {
            Data[offset] = rgba[0];
            Data[offset + 1] = rgba[1];
            Data[offset + 2] = rgba[2];
            Data[offset + 3] = rgba[3];
        }
    }
};

class IblRenderer {
private:
    std::optional<ImageRgba16f> m_image;
    std::shared_ptr<tekki::backend::vulkan::Image> m_texture;

    ImageRgba16f LoadImageFromFile(const std::filesystem::path& path);
    ImageRgba16f LoadHdr(const std::filesystem::path& filePath);
    ImageRgba16f LoadExr(const std::filesystem::path& filePath);

public:
    IblRenderer();
    void UnloadImage();
    void LoadImage(const std::filesystem::path& path);
    std::optional<tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image>> Render(
        tekki::render_graph::TemporalRenderGraph& renderGraph);
};

}