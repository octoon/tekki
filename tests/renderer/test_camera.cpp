#include <catch2/catch_test_macros.hpp>
#include <tekki/renderer/camera.h>
#include <glm/gtc/matrix_transform.hpp>

using namespace tekki::renderer;

// Note: Camera tests require proper initialization
// These are placeholder tests. Full camera tests in integration suite.

TEST_CASE("Camera math operations", "[renderer][camera]") {
    SECTION("GLM matrix operations") {
        glm::mat4 identity = glm::mat4(1.0f);
        glm::vec4 v(1.0f, 2.0f, 3.0f, 1.0f);
        glm::vec4 result = identity * v;

        REQUIRE(result == v);
    }

    SECTION("GLM transformations") {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        glm::vec4 v(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = translation * v;

        REQUIRE(result.x == Catch::Approx(1.0f));
    }
}
