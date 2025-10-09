#include "../../include/tekki/renderer/default_world_renderer.h"
#include "../../include/tekki/renderer/lut_renderers.h"
#include "../../include/tekki/core/log.h"

namespace tekki::renderer {

std::unique_ptr<world::WorldRenderer> create_default_world_renderer(
    const std::array<uint32_t, 2>& render_extent,
    const std::array<uint32_t, 2>& temporal_upscale_extent,
    std::shared_ptr<vulkan::Device> device
) {
    // Create empty world renderer
    auto world_renderer = std::make_unique<world::WorldRenderer>(
        render_extent,
        temporal_upscale_extent,
        device
    );

    // Add BRDF FG LUT (index 0)
    {
        BrdfFgLutComputer brdf_computer;
        auto brdf_lut_image = brdf_computer.create(device.get());

        // Create a temporary render graph to compute the LUT
        // TODO: This should be done in a proper initialization pass
        // For now, we'll add it as a placeholder
        world_renderer->add_image_lut(std::move(brdf_computer), 0);

        TEKKI_LOG_INFO("Created BRDF FG LUT (index 0)");
    }

    // Add blue noise texture (index 1)
    {
        // TODO: Load blue noise texture from file
        // "/images/bluenoise/256_256/LDR_RGBA_0.png"
        // For now, create a placeholder texture

        auto blue_noise_desc = vulkan::ImageDesc::new_2d(256, 256, vk::Format::eR8G8B8A8Unorm)
            .usage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst);

        auto blue_noise_img = device->create_image(blue_noise_desc, "blue_noise_256");

        auto handle = world_renderer->add_image(blue_noise_img);

        // Verify it's at index 1 (BINDLESS_LUT_BLUE_NOISE_256_LDR_RGBA_0)
        if (handle.id != 1) {
            TEKKI_LOG_WARN("Blue noise texture index mismatch: expected 1, got {}", handle.id);
        }

        TEKKI_LOG_INFO("Created blue noise texture (index 1)");
    }

    // Add Bezold-Brücke LUT (index 2)
    {
        BezoldBruckeLutComputer bb_computer;
        auto bb_lut_image = bb_computer.create(device.get());

        world_renderer->add_image_lut(std::move(bb_computer), 2);

        TEKKI_LOG_INFO("Created Bezold-Brücke LUT (index 2)");
    }

    // Build empty TLAS if ray tracing is enabled
    if (device->ray_tracing_enabled()) {
        world_renderer->build_ray_tracing_top_level_acceleration();
        TEKKI_LOG_INFO("Built initial ray tracing acceleration structure");
    }

    return world_renderer;
}

} // namespace tekki::renderer
