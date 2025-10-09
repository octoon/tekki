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

public:
    IblRenderer() : m_image(std::nullopt), m_texture(nullptr) {}

    void UnloadImage() {
        m_image = std::nullopt;
        m_texture = nullptr;
    }

    void LoadImage(const std::filesystem::path& path) {
        m_image = LoadImageFromFile(path);
        m_texture = nullptr;
    }

    std::optional<tekki::render_graph::ReadOnlyHandle<tekki::backend::vulkan::Image>> Render(
        tekki::render_graph::TemporalRenderGraph& renderGraph) {
        if (!m_texture) {
            constexpr uint32_t PIXEL_BYTES = 8;

            if (m_image) {
                auto image = std::move(m_image.value());
                m_image = std::nullopt;

                try {
                    tekki::backend::vulkan::ImageDesc desc;
                    desc.ImageType = tekki::backend::vulkan::ImageType::Tex2d;
                    desc.Format = VK_FORMAT_R16G16B16A16_SFLOAT;
                    desc.Width = image.Size.x;
                    desc.Height = image.Size.y;
                    desc.Depth = 1;
                    desc.ArrayLayers = 1;
                    desc.MipLevels = 1;
                    desc.Usage = VK_IMAGE_USAGE_SAMPLED_BIT;
                    desc.Flags = 0;

                    std::vector<tekki::backend::vulkan::ImageSubResourceData> subresourceData;
                    tekki::backend::vulkan::ImageSubResourceData data;
                    data.Data = image.Data.data();
                    data.RowPitch = image.Size.x * PIXEL_BYTES;
                    data.SlicePitch = image.Size.x * image.Size.y * PIXEL_BYTES;
                    subresourceData.push_back(data);

                    m_texture = renderGraph.GetDevice()->CreateImage(desc, subresourceData);
                } catch (const std::exception& e) {
                    throw std::runtime_error(std::string("Failed to create image: ") + e.what());
                }
            }
        }

        if (m_texture) {
            constexpr uint32_t width = 1024;
            auto cubeTex = renderGraph.Create(tekki::backend::vulkan::ImageDesc{
                tekki::backend::vulkan::ImageType::Cube,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                0,
                VK_FORMAT_R16G16B16A16_SFLOAT,
                width,
                width,
                1,
                6,
                1
            });

            auto texture = renderGraph.Import(
                m_texture,
                vk_sync::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
            );

            auto pass = renderGraph.AddPass("ibl cube");
            tekki::render_graph::SimpleRenderPass simplePass(pass, "/shaders/ibl/ibl_cube.hlsl");
            simplePass.Read(texture);
            
            tekki::backend::vulkan::ImageViewDesc viewDesc;
            viewDesc.ViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            simplePass.WriteView(cubeTex, viewDesc);
            
            simplePass.Constants(width);
            simplePass.Dispatch(glm::uvec3(width, width, 6));

            return cubeTex;
        }

        return std::nullopt;
    }

private:
    ImageRgba16f LoadImageFromFile(const std::filesystem::path& path) {
        std::string extension = path.extension().string();
        
        if (extension == ".exr") {
            return LoadExr(path);
        } else if (extension == ".hdr") {
            return LoadHdr(path);
        } else {
            throw std::runtime_error("Unsupported file extension: " + extension);
        }
    }

    ImageRgba16f LoadHdr(const std::filesystem::path& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open specified file: " + filePath.string());
        }

        // HDR loading implementation would go here
        // For now, return a placeholder implementation
        ImageRgba16f image(1, 1);
        image.PutPixel(0, 0, {0x3C00, 0x3C00, 0x3C00, 0x3C00}); // 1.0 in f16 for all channels
        return image;
    }

    ImageRgba16f LoadExr(const std::filesystem::path& filePath) {
        // EXR loading implementation would go here
        // For now, return a placeholder implementation
        ImageRgba16f image(1, 1);
        image.PutPixel(0, 0, {0x3C00, 0x3C00, 0x3C00, 0x3C00}); // 1.0 in f16 for all channels
        return image;
    }
};

}