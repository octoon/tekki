#include <catch2/catch_test_macros.hpp>
#include <tekki/asset/mesh.h>
#include <glm/glm.hpp>

using namespace tekki::asset;

// Note: Mesh loading requires glTF files
// These are basic structure tests. Full tests with real meshes are in integration suite.

TEST_CASE("Mesh structure", "[asset][mesh]") {
    SECTION("GLM vector sizes") {
        REQUIRE(sizeof(glm::vec3) == 3 * sizeof(float));
        REQUIRE(sizeof(glm::vec2) == 2 * sizeof(float));
        REQUIRE(sizeof(glm::vec4) == 4 * sizeof(float));
    }

    SECTION("Vector operations") {
        glm::vec3 v1(1.0f, 0.0f, 0.0f);
        glm::vec3 v2(0.0f, 1.0f, 0.0f);

        float dot = glm::dot(v1, v2);
        REQUIRE(dot == 0.0f); // Orthogonal vectors
    }
}
