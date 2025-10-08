#pragma once

#include <glm/glm.hpp>

namespace tekki::shared {

struct CameraMatrices {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 view_projection;
    glm::mat4 inv_view;
    glm::mat4 inv_projection;
    glm::mat4 inv_view_projection;
    glm::vec4 eye_position;
    glm::vec4 viewport_size;

    CameraMatrices() = default;

    CameraMatrices(
        const glm::mat4& view,
        const glm::mat4& projection,
        const glm::vec4& eye_position,
        const glm::vec4& viewport_size
    ) : view(view), projection(projection), eye_position(eye_position), viewport_size(viewport_size) {
        view_projection = projection * view;
        inv_view = glm::inverse(view);
        inv_projection = glm::inverse(projection);
        inv_view_projection = glm::inverse(view_projection);
    }
};

} // namespace tekki::shared