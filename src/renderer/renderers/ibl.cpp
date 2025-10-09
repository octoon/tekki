#include "../../../include/tekki/renderer/renderers/ibl.h"
#include "../../../include/tekki/backend/vulkan/shader.h"
#include "../../../include/tekki/core/log.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include <ImfArray.h>
#include <ImfRgbaFile.h>
#include <ImathBox.h>
#include <half/half.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace tekki::renderer {

namespace {

constexpr float HALF_MAX_VALUE = 65504.0f;

uint16_t float_to_half_bits(float value) {
    const float clamped = std::clamp(value, -HALF_MAX_VALUE, HALF_MAX_VALUE);
    const half_float::half half_value(clamped);
    uint16_t bits{};
    std::memcpy(&bits, &half_value, sizeof(bits));
    return bits;
}

} // namespace

// ImageRgba16f implementation

ImageRgba16f::ImageRgba16f(uint32_t width, uint32_t height)
    : size{width, height}, data(width * height * 4, 0) {}

void ImageRgba16f::put_pixel(uint32_t x, uint32_t y, const std::array<uint16_t, 4>& rgba) {
    size_t offset = ((y * size[0] + x) * 4);
    std::memcpy(&data[offset], rgba.data(), 4 * sizeof(uint16_t));
}

// IblRenderer implementation

void IblRenderer::load_image(const std::string& path) {
    try {
        // Determine file type by extension
        std::string ext;
        size_t dot_pos = path.find_last_of('.');
        if (dot_pos != std::string::npos) {
            ext = path.substr(dot_pos + 1);
            // Convert to lowercase
            for (auto& c : ext) {
                c = std::tolower(c);
            }
        }

        if (ext == "exr") {
            image_ = load_exr(path);
        } else if (ext == "hdr") {
            image_ = load_hdr(path);
        } else {
            TEKKI_LOG_ERROR("Unsupported IBL image format: {}", ext);
            return;
        }

        // Force re-creation of the texture
        texture_ = nullptr;

        TEKKI_LOG_INFO("Loaded IBL environment map: {}", path);
    } catch (const std::exception& e) {
        TEKKI_LOG_ERROR("Failed to load IBL image {}: {}", path, e.what());
        image_ = std::nullopt;
    }
}

void IblRenderer::unload_image() {
    image_ = std::nullopt;
    texture_ = nullptr;
}

std::optional<render_graph::ReadOnlyHandle<vulkan::Image>> IblRenderer::render(
    render_graph::TemporalRenderGraph& rg
) {
    // Create texture from image if needed
    if (!texture_ && image_.has_value()) {
        const uint32_t PIXEL_BYTES = 8;  // RGBA16F = 4 channels * 2 bytes
        auto& img = image_.value();

        auto desc = vulkan::ImageDesc::new_2d(img.size[0], img.size[1], vk::Format::eR16G16B16A16Sfloat)
            .usage(vk::ImageUsageFlagBits::eSampled);

        std::vector<vulkan::ImageSubResourceData> subresources = {
            vulkan::ImageSubResourceData{
                img.data.data(),
                static_cast<size_t>(img.size[0] * PIXEL_BYTES),
                static_cast<size_t>(img.size[0] * img.size[1] * PIXEL_BYTES)
            }
        };

        texture_ = rg.device()->create_image(desc, "ibl_environment", subresources);
    }

    if (!texture_) {
        return std::nullopt;
    }

    // Create cube map from environment map
    const uint32_t width = 1024;
    auto cube_desc = vulkan::ImageDesc::new_cube(width, vk::Format::eR16G16B16A16Sfloat);
    auto cube_tex = rg.create(cube_desc);

    // Import the texture into render graph
    auto texture_handle = rg.import(
        texture_,
        vulkan::AccessType::AnyShaderReadSampledImageOrUniformTexelBuffer
    );

    // Create compute pass to generate cube map
    auto pass = rg.add_pass("ibl cube");

    // Register compute shader
    auto shader_desc = vulkan::PipelineShaderDesc::builder(vulkan::ShaderPipelineStage::Compute)
        .hlsl_source("/shaders/ibl/ibl_cube.hlsl")
        .build();

    auto pipeline = pass.register_compute_pipeline({shader_desc});

    // Register reads and writes
    auto texture_ref = pass.read(
        texture_handle,
        vulkan::AccessType::ComputeShaderReadSampledImageOrUniformTexelBuffer
    );

    auto view_desc = vulkan::ImageViewDesc::builder()
        .view_type(vk::ImageViewType::e2DArray)
        .build();

    auto cube_ref = pass.write_view(
        cube_tex,
        view_desc,
        vulkan::AccessType::ComputeShaderWrite
    );

    // Dispatch callback
    pass.dispatch([=](render_graph::PassApi& api) {
        // Bind pipeline
        auto bound_pipeline = api.bind_compute_pipeline(
            pipeline
                .into_binding()
                .descriptor_set(0, {
                    render_graph::RenderPassBinding::Image(texture_ref),
                    render_graph::RenderPassBinding::Image(cube_ref)
                })
                .constants(&width, sizeof(width))
        );

        // Dispatch for all 6 cube faces
        api.cb().dispatch(width, width, 6);
    });

    return render_graph::ReadOnlyHandle<vulkan::Image>(cube_tex);
}

// Image loading helper functions

ImageRgba16f load_hdr(const std::string& path) {
    int width = 0;
    int height = 0;
    int components = 0;

    stbi_set_flip_vertically_on_load(false);
    float* pixels = stbi_loadf(path.c_str(), &width, &height, &components, 3);
    if (!pixels) {
        throw std::runtime_error(
            std::string("Failed to load HDR image ") + path + ": " + stbi_failure_reason()
        );
    }

    ImageRgba16f image(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    const uint16_t alpha_bits = float_to_half_bits(1.0f);

    const size_t stride = static_cast<size_t>(width) * 3;
    for (int y = 0; y < height; ++y) {
        const float* row = pixels + static_cast<size_t>(y) * stride;
        for (int x = 0; x < width; ++x) {
            const float* px = row + static_cast<size_t>(x) * 3;
            std::array<uint16_t, 4> rgba = {
                float_to_half_bits(px[0]),
                float_to_half_bits(px[1]),
                float_to_half_bits(px[2]),
                alpha_bits,
            };
            image.put_pixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), rgba);
        }
    }

    stbi_image_free(pixels);
    return image;
}

ImageRgba16f load_exr(const std::string& path) {
    try {
        OPENEXR_IMF_NAMESPACE::RgbaInputFile file(path.c_str());
        const Imath::Box2i dw = file.dataWindow();
        const int width = dw.max.x - dw.min.x + 1;
        const int height = dw.max.y - dw.min.y + 1;

        OPENEXR_IMF_NAMESPACE::Array2D<OPENEXR_IMF_NAMESPACE::Rgba> pixels;
        pixels.resizeErase(height, width);

        file.setFrameBuffer(
            &pixels[0][0] - dw.min.x - dw.min.y * width,
            1,
            width
        );
        file.readPixels(dw.min.y, dw.max.y);

        ImageRgba16f image(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        const uint16_t alpha_bits = float_to_half_bits(1.0f);

        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const auto& px = pixels[y][x];
                std::array<uint16_t, 4> rgba = {
                    static_cast<uint16_t>(px.r.bits()),
                    static_cast<uint16_t>(px.g.bits()),
                    static_cast<uint16_t>(px.b.bits()),
                    alpha_bits,
                };
                image.put_pixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), rgba);
            }
        }

        return image;
    } catch (const OPENEXR_IMF_NAMESPACE::BaseExc& e) {
        throw std::runtime_error(std::string("Failed to load EXR image ") + path + ": " + e.what());
    }
}

} // namespace tekki::renderer
