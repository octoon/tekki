#include "tekki/rust_shaders/atmosphere.h"

namespace tekki::rust_shaders {

glm::vec3 integrate_optical_depth(const glm::vec3& ray_o, const glm::vec3& ray_d) {
    glm::vec2 intersection = atmosphere_intersection(ray_o, ray_d);
    float ray_length = intersection.y;

    int sample_count = 8;
    float step_size = ray_length / static_cast<float>(sample_count);

    glm::vec3 optical_depth = glm::vec3(0.0f);

    int i = 0;
    // Using a while loop here as a workaround for a spirv-cross bug.
    // See https://github.com/EmbarkStudios/rust-gpu/issues/739
    while (i < sample_count) {
        glm::vec3 local_pos = ray_o + ray_d * (static_cast<float>(i) + 0.5f) * step_size;
        float local_height = atmosphere_height(local_pos);
        glm::vec3 local_density = atmosphere_density(local_height);

        optical_depth += local_density * step_size;

        i++;
    }

    return optical_depth;
}

float atmosphere_height(const glm::vec3& position_ws) {
    return glm::length(position_ws - PLANET_CENTER) - PLANET_RADIUS;
}

float density_rayleigh(float h) {
    return std::exp(-std::max(0.0f, h / RAYLEIGH_HEIGHT));
}

float density_mie(float h) {
    return std::exp(-std::max(0.0f, h / MIE_HEIGHT));
}

float density_ozone(float h) {
    // The ozone layer is represented as a tent function with a width of 30km, centered around an altitude of 25km.
    return std::max(0.0f, 1.0f - std::abs(h - 25000.0f) / 15000.0f);
}

glm::vec3 atmosphere_density(float h) {
    return glm::vec3(density_rayleigh(h), density_mie(h), density_ozone(h));
}

glm::vec2 sphere_intersection(glm::vec3 ray_o, const glm::vec3& ray_d, const glm::vec3& sphere_o, float sphere_r) {
    ray_o -= sphere_o;
    float a = glm::dot(ray_d, ray_d);
    float b = 2.0f * glm::dot(ray_o, ray_d);
    float c = glm::dot(ray_o, ray_o) - sphere_r * sphere_r;
    float d = b * b - 4.0f * a * c;
    if (d < 0.0f) {
        return glm::vec2(-1.0f);
    } else {
        d = std::sqrt(d);
        return glm::vec2(-b - d, -b + d) / (2.0f * a);
    }
}

glm::vec2 atmosphere_intersection(const glm::vec3& ray_o, const glm::vec3& ray_d) {
    return sphere_intersection(
        ray_o,
        ray_d,
        PLANET_CENTER,
        PLANET_RADIUS + ATMOSPHERE_HEIGHT
    );
}

// -------------------------------------
// Phase functions
float phase_rayleigh(float costh) {
    return 3.0f * (1.0f + costh * costh) / (16.0f * PI);
}

float phase_mie(float costh, float g) {
    // g = 0.85)
    g = std::min(g, 0.9381f);
    float k = 1.55f * g - 0.55f * g * g * g;
    float kcosth = k * costh;
    return (1.0f - k * k) / ((4.0f * PI) * (1.0f - kcosth) * (1.0f - kcosth));
}

/// Calculate a luminance transmittance value from optical depth.
glm::vec3 absorb(const glm::vec3& optical_depth) {
    // Note that Mie results in slightly more light absorption than scattering, about 10%
    return glm::exp(-(optical_depth.x * C_RAYLEIGH + optical_depth.y * C_MIE * 1.1f + optical_depth.z * C_OZONE)
        * ATMOSPHERE_DENSITY);
}

// Integrate scattering over a ray for a single directional light source.
// Also return the transmittance for the same ray as we are already calculating the optical depth anyway.
glm::vec3 integrate_scattering(
    glm::vec3 ray_start,
    const glm::vec3& ray_dir,
    float ray_length,
    const glm::vec3& light_dir,
    const glm::vec3& light_color,
    glm::vec3& transmittance
) {
    // We can reduce the number of atmospheric samples required to converge by spacing them exponentially closer to the camera.
    // This breaks space view however, so let's compensate for that with an exponent that "fades" to 1 as we leave the atmosphere.
    // float ray_height = atmosphere_height(ray_start);
    // float sample_distribution_exponent = 1 + std::clamp(1 - ray_height / ATMOSPHERE_HEIGHT, 0.0f, 1.0f) * 8; // Slightly arbitrary max exponent of 9
    // float sample_distribution_exponent = 1 + 8 * std::abs(ray_dir.y);
    float sample_distribution_exponent = 5.0f;

    glm::vec2 intersection = atmosphere_intersection(ray_start, ray_dir);

    ray_length = std::min(ray_length, intersection.y);
    if (intersection.x > 0.0f) {
        // Advance ray to the atmosphere entry point
        ray_start += ray_dir * intersection.x;
        ray_length -= intersection.x;
    }

    float costh = glm::dot(ray_dir, light_dir);
    float phase_r = phase_rayleigh(costh);
    float phase_m = phase_mie(costh, 0.85f);

    int sample_count = 16;

    glm::vec3 optical_depth = glm::vec3(0.0f);
    glm::vec3 rayleigh = glm::vec3(0.0f);
    glm::vec3 mie = glm::vec3(0.0f);

    float prev_ray_time = 0.0f;

    for (int i = 1; i <= sample_count; i++) {
        float ray_time = std::pow(static_cast<float>(i) / static_cast<float>(sample_count), sample_distribution_exponent) * ray_length;
        // Because we are distributing the samples exponentially, we have to calculate the step size per sample.
        float step_size = ray_time - prev_ray_time;

        // glm::vec3 local_position = ray_start + ray_dir * ray_time;
        glm::vec3 local_position = ray_start + ray_dir * glm::mix(prev_ray_time, ray_time, 0.5f);
        float local_height = atmosphere_height(local_position);
        glm::vec3 local_density = atmosphere_density(local_height);

        optical_depth += local_density * step_size;

        // The atmospheric transmittance from ray_start to local_position
        glm::vec3 view_transmittance = absorb(optical_depth);

        glm::vec3 optical_depthlight = integrate_optical_depth(local_position, light_dir);

        // The atmospheric transmittance of light reaching local_position
        glm::vec3 light_transmittance = absorb(optical_depthlight);

        rayleigh += view_transmittance * light_transmittance * phase_r * local_density.x * step_size;
        mie += view_transmittance * light_transmittance * phase_m * local_density.y * step_size;

        prev_ray_time = ray_time;
    }

    transmittance = absorb(optical_depth);

    return (rayleigh * C_RAYLEIGH + mie * C_MIE) * light_color * EXPOSURE;
}

} // namespace tekki::rust_shaders