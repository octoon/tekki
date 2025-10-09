#include "../../../include/tekki/renderer/renderers/ibl.h"
#include "../../../include/tekki/backend/vulkan/shader.h"
#include "../../../include/tekki/core/log.h"

#include <fstream>
#include <cstring>

namespace tekki::renderer {

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
    // TODO: Implement HDR loading using stb_image or similar
    // For now, return a placeholder
    TEKKI_LOG_WARN("HDR loading not yet implemented, returning placeholder");
    return ImageRgba16f(1, 1);
}

ImageRgba16f load_exr(const std::string& path) {
    // TODO: Implement EXR loading using OpenEXR or tinyexr
    // For now, return a placeholder
    TEKKI_LOG_WARN("EXR loading not yet implemented, returning placeholder");
    return ImageRgba16f(1, 1);
}

} // namespace tekki::renderer
