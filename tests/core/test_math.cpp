#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Basic GLM integration tests
TEST_CASE("GLM math library integration", "[core][math]") {
    SECTION("Vector operations") {
        glm::vec3 v1(1.0f, 2.0f, 3.0f);
        glm::vec3 v2(4.0f, 5.0f, 6.0f);

        glm::vec3 sum = v1 + v2;
        REQUIRE_THAT(sum.x, Catch::Matchers::WithinRel(5.0f, 0.001f));
        REQUIRE_THAT(sum.y, Catch::Matchers::WithinRel(7.0f, 0.001f));
        REQUIRE_THAT(sum.z, Catch::Matchers::WithinRel(9.0f, 0.001f));

        float dot_product = glm::dot(v1, v2);
        REQUIRE_THAT(dot_product, Catch::Matchers::WithinRel(32.0f, 0.001f));

        glm::vec3 cross_product = glm::cross(v1, v2);
        REQUIRE_THAT(cross_product.x, Catch::Matchers::WithinRel(-3.0f, 0.001f));
        REQUIRE_THAT(cross_product.y, Catch::Matchers::WithinRel(6.0f, 0.001f));
        REQUIRE_THAT(cross_product.z, Catch::Matchers::WithinRel(-3.0f, 0.001f));
    }

    SECTION("Matrix operations") {
        glm::mat4 identity = glm::mat4(1.0f);
        glm::vec4 v(1.0f, 2.0f, 3.0f, 1.0f);

        glm::vec4 result = identity * v;
        REQUIRE_THAT(result.x, Catch::Matchers::WithinRel(1.0f, 0.001f));
        REQUIRE_THAT(result.y, Catch::Matchers::WithinRel(2.0f, 0.001f));
        REQUIRE_THAT(result.z, Catch::Matchers::WithinRel(3.0f, 0.001f));
        REQUIRE_THAT(result.w, Catch::Matchers::WithinRel(1.0f, 0.001f));
    }

    SECTION("Transformations") {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        glm::vec4 point(0.0f, 0.0f, 0.0f, 1.0f);

        glm::vec4 transformed = translation * point;
        REQUIRE_THAT(transformed.x, Catch::Matchers::WithinRel(1.0f, 0.001f));
        REQUIRE_THAT(transformed.y, Catch::Matchers::WithinRel(2.0f, 0.001f));
        REQUIRE_THAT(transformed.z, Catch::Matchers::WithinRel(3.0f, 0.001f));
    }
}
