#include <catch2/catch_test_macros.hpp>
#include <tekki/renderer/math.h>
#include <glm/glm.hpp>

using namespace tekki::renderer;

TEST_CASE("Math basic operations", "[renderer][math]") {
    SECTION("Clamp value") {
        REQUIRE(Clamp(5.0f, 0.0f, 10.0f) == 5.0f);
        REQUIRE(Clamp(-5.0f, 0.0f, 10.0f) == 0.0f);
        REQUIRE(Clamp(15.0f, 0.0f, 10.0f) == 10.0f);
    }

    SECTION("Lerp") {
        REQUIRE(Lerp(0.0f, 10.0f, 0.0f) == 0.0f);
        REQUIRE(Lerp(0.0f, 10.0f, 1.0f) == 10.0f);
        REQUIRE(Lerp(0.0f, 10.0f, 0.5f) == Catch::Approx(5.0f));
    }

    SECTION("Smoothstep") {
        REQUIRE(Smoothstep(0.0f, 1.0f, 0.0f) == Catch::Approx(0.0f));
        REQUIRE(Smoothstep(0.0f, 1.0f, 1.0f) == Catch::Approx(1.0f));
        REQUIRE(Smoothstep(0.0f, 1.0f, 0.5f) == Catch::Approx(0.5f));
    }

    SECTION("Saturate") {
        REQUIRE(Saturate(-0.5f) == 0.0f);
        REQUIRE(Saturate(0.5f) == 0.5f);
        REQUIRE(Saturate(1.5f) == 1.0f);
    }
}

TEST_CASE("Vector operations", "[renderer][math]") {
    SECTION("Vector length") {
        glm::vec3 v(3.0f, 4.0f, 0.0f);
        REQUIRE(glm::length(v) == Catch::Approx(5.0f));
    }

    SECTION("Vector normalization") {
        glm::vec3 v(3.0f, 4.0f, 0.0f);
        glm::vec3 normalized = glm::normalize(v);

        REQUIRE(glm::length(normalized) == Catch::Approx(1.0f));
    }

    SECTION("Dot product") {
        glm::vec3 a(1.0f, 0.0f, 0.0f);
        glm::vec3 b(1.0f, 0.0f, 0.0f);

        REQUIRE(glm::dot(a, b) == Catch::Approx(1.0f));
    }

    SECTION("Cross product") {
        glm::vec3 a(1.0f, 0.0f, 0.0f);
        glm::vec3 b(0.0f, 1.0f, 0.0f);
        glm::vec3 c = glm::cross(a, b);

        REQUIRE(c.x == Catch::Approx(0.0f));
        REQUIRE(c.y == Catch::Approx(0.0f));
        REQUIRE(c.z == Catch::Approx(1.0f));
    }

    SECTION("Reflect vector") {
        glm::vec3 incident(1.0f, -1.0f, 0.0f);
        incident = glm::normalize(incident);
        glm::vec3 normal(0.0f, 1.0f, 0.0f);

        glm::vec3 reflected = Reflect(incident, normal);

        REQUIRE(reflected.x > 0.0f);
        REQUIRE(reflected.y > 0.0f); // Should bounce up
    }

    SECTION("Refract vector") {
        glm::vec3 incident(0.0f, -1.0f, 0.0f);
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        float eta = 1.0f / 1.5f; // Air to glass

        glm::vec3 refracted = Refract(incident, normal, eta);

        REQUIRE(glm::length(refracted) > 0.0f);
    }
}

TEST_CASE("Matrix operations", "[renderer][math]") {
    SECTION("Identity matrix") {
        glm::mat4 identity = glm::mat4(1.0f);

        glm::vec4 v(1.0f, 2.0f, 3.0f, 1.0f);
        glm::vec4 result = identity * v;

        REQUIRE(result == v);
    }

    SECTION("Translation matrix") {
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));

        glm::vec4 v(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = translation * v;

        REQUIRE(result.x == Catch::Approx(1.0f));
        REQUIRE(result.y == Catch::Approx(2.0f));
        REQUIRE(result.z == Catch::Approx(3.0f));
    }

    SECTION("Rotation matrix") {
        glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        glm::vec4 v(1.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 result = rotation * v;

        REQUIRE(result.x == Catch::Approx(0.0f).margin(0.001f));
        REQUIRE(result.y == Catch::Approx(1.0f).margin(0.001f));
    }

    SECTION("Scale matrix") {
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f, 3.0f, 4.0f));

        glm::vec4 v(1.0f, 1.0f, 1.0f, 1.0f);
        glm::vec4 result = scale * v;

        REQUIRE(result.x == Catch::Approx(2.0f));
        REQUIRE(result.y == Catch::Approx(3.0f));
        REQUIRE(result.z == Catch::Approx(4.0f));
    }

    SECTION("Matrix inverse") {
        glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        glm::mat4 inv = glm::inverse(m);

        glm::mat4 identity = m * inv;

        // Should be close to identity
        REQUIRE(identity[0][0] == Catch::Approx(1.0f));
        REQUIRE(identity[1][1] == Catch::Approx(1.0f));
        REQUIRE(identity[2][2] == Catch::Approx(1.0f));
        REQUIRE(identity[3][3] == Catch::Approx(1.0f));
    }
}

TEST_CASE("Quaternion operations", "[renderer][math]") {
    SECTION("Identity quaternion") {
        glm::quat q = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 v(1.0f, 0.0f, 0.0f);
        glm::vec3 rotated = q * v;

        REQUIRE(rotated == v);
    }

    SECTION("Quaternion rotation") {
        // 90 degree rotation around Z axis
        glm::quat q = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        glm::vec3 v(1.0f, 0.0f, 0.0f);
        glm::vec3 rotated = q * v;

        REQUIRE(rotated.x == Catch::Approx(0.0f).margin(0.001f));
        REQUIRE(rotated.y == Catch::Approx(1.0f).margin(0.001f));
        REQUIRE(rotated.z == Catch::Approx(0.0f).margin(0.001f));
    }

    SECTION("Quaternion to matrix") {
        glm::quat q = glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 m = glm::mat4_cast(q);

        REQUIRE(m != glm::mat4(1.0f));
    }

    SECTION("Slerp between quaternions") {
        glm::quat q1 = glm::angleAxis(glm::radians(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::quat q2 = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::quat mid = glm::slerp(q1, q2, 0.5f);

        // Should be roughly 45 degrees
        glm::vec3 axis;
        float angle;
        angle = glm::angle(mid);
        axis = glm::axis(mid);

        REQUIRE(angle == Catch::Approx(glm::radians(45.0f)));
    }
}

TEST_CASE("Bounding box operations", "[renderer][math]") {
    SECTION("Create bounding box") {
        BoundingBox bbox;
        bbox.Min = glm::vec3(-1.0f, -1.0f, -1.0f);
        bbox.Max = glm::vec3(1.0f, 1.0f, 1.0f);

        REQUIRE(bbox.GetCenter() == glm::vec3(0.0f, 0.0f, 0.0f));
        REQUIRE(bbox.GetSize() == glm::vec3(2.0f, 2.0f, 2.0f));
    }

    SECTION("Point inside bounding box") {
        BoundingBox bbox;
        bbox.Min = glm::vec3(-1.0f, -1.0f, -1.0f);
        bbox.Max = glm::vec3(1.0f, 1.0f, 1.0f);

        REQUIRE(bbox.Contains(glm::vec3(0.0f, 0.0f, 0.0f)));
        REQUIRE(bbox.Contains(glm::vec3(0.5f, 0.5f, 0.5f)));
        REQUIRE_FALSE(bbox.Contains(glm::vec3(2.0f, 0.0f, 0.0f)));
    }

    SECTION("Bounding box intersection") {
        BoundingBox bbox1;
        bbox1.Min = glm::vec3(0.0f, 0.0f, 0.0f);
        bbox1.Max = glm::vec3(2.0f, 2.0f, 2.0f);

        BoundingBox bbox2;
        bbox2.Min = glm::vec3(1.0f, 1.0f, 1.0f);
        bbox2.Max = glm::vec3(3.0f, 3.0f, 3.0f);

        REQUIRE(bbox1.Intersects(bbox2));
    }

    SECTION("Expand bounding box") {
        BoundingBox bbox;
        bbox.Min = glm::vec3(0.0f, 0.0f, 0.0f);
        bbox.Max = glm::vec3(1.0f, 1.0f, 1.0f);

        bbox.Expand(glm::vec3(2.0f, 2.0f, 2.0f));

        REQUIRE(bbox.Max == glm::vec3(2.0f, 2.0f, 2.0f));
    }
}

TEST_CASE("Ray operations", "[renderer][math]") {
    SECTION("Ray construction") {
        Ray ray;
        ray.Origin = glm::vec3(0.0f, 0.0f, 0.0f);
        ray.Direction = glm::vec3(0.0f, 0.0f, -1.0f);

        REQUIRE(ray.Origin == glm::vec3(0.0f, 0.0f, 0.0f));
        REQUIRE(ray.Direction == glm::vec3(0.0f, 0.0f, -1.0f));
    }

    SECTION("Ray point at distance") {
        Ray ray;
        ray.Origin = glm::vec3(0.0f, 0.0f, 0.0f);
        ray.Direction = glm::vec3(0.0f, 0.0f, -1.0f);

        glm::vec3 point = ray.PointAt(5.0f);

        REQUIRE(point == glm::vec3(0.0f, 0.0f, -5.0f));
    }

    SECTION("Ray-sphere intersection") {
        Ray ray;
        ray.Origin = glm::vec3(0.0f, 0.0f, 5.0f);
        ray.Direction = glm::vec3(0.0f, 0.0f, -1.0f);

        glm::vec3 sphere_center(0.0f, 0.0f, 0.0f);
        float sphere_radius = 1.0f;

        auto hit = ray.IntersectSphere(sphere_center, sphere_radius);

        REQUIRE(hit.has_value());
        REQUIRE(*hit == Catch::Approx(4.0f)); // Distance to intersection
    }
}
