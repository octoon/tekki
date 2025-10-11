#include <catch2/catch_test_macros.hpp>
#include <tekki/renderer/buffer_builder.h>
#include <glm/glm.hpp>

using namespace tekki::renderer;

// Note: BufferBuilder tests require proper initialization
// These are placeholder tests. Full tests in integration suite.

TEST_CASE("BufferBuilder placeholder", "[renderer][buffer_builder]") {
    SECTION("Type sizes") {
        REQUIRE(sizeof(int) == 4);
        REQUIRE(sizeof(float) == 4);
        REQUIRE(sizeof(double) == 8);
    }

    SECTION("GLM type sizes") {
        REQUIRE(sizeof(glm::vec3) == 3 * sizeof(float));
        REQUIRE(sizeof(glm::vec4) == 4 * sizeof(float));
        REQUIRE(sizeof(glm::mat4) == 16 * sizeof(float));
    }
}
