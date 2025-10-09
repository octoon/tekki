#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace tekki::renderer {

using glm::mat2;
using glm::mat3;
using glm::mat4;
using glm::quat;
using glm::vec2;
using glm::vec3;
using glm::vec4;

/**
 * Builds an orthonormal basis from a given normal vector.
 * 
 * @param n The normal vector to build the basis from
 * @return A 3x3 matrix representing the orthonormal basis
 */
inline mat3 BuildOrthonormalBasis(const vec3& n) {
    vec3 b1;
    vec3 b2;

    if (n.z < 0.0f) {
        float a = 1.0f / (1.0f - n.z);
        float b = n.x * n.y * a;
        b1 = vec3(1.0f - n.x * n.x * a, -b, n.x);
        b2 = vec3(b, n.y * n.y * a - 1.0f, -n.y);
    } else {
        float a = 1.0f / (1.0f + n.z);
        float b = -n.x * n.y * a;
        b1 = vec3(1.0f - n.x * n.x * a, b, -n.x);
        b2 = vec3(b, 1.0f - n.y * n.y * a, -n.y);
    }

    return mat3(b1, b2, n);
}

} // namespace tekki::renderer