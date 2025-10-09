#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <tekki/renderer/world/WorldRenderer.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace tekki::renderer::world;

TEST_CASE("MeshHandle operations", "[renderer][world]") {
    SECTION("MeshHandle creation and comparison") {
        MeshHandle h1(0);
        MeshHandle h2(0);
        MeshHandle h3(1);

        REQUIRE(h1 == h2);
        REQUIRE(h1 != h3);
    }

    SECTION("Invalid MeshHandle") {
        MeshHandle invalid = MeshHandle::INVALID;
        REQUIRE(invalid.is_invalid());

        MeshHandle valid(10);
        REQUIRE_FALSE(valid.is_invalid());
    }

    SECTION("MeshHandle as map key") {
        std::unordered_map<MeshHandle, int> mesh_map;
        MeshHandle h1(1);
        MeshHandle h2(2);

        mesh_map[h1] = 100;
        mesh_map[h2] = 200;

        REQUIRE(mesh_map[h1] == 100);
        REQUIRE(mesh_map[h2] == 200);
    }
}

TEST_CASE("InstanceHandle operations", "[renderer][world]") {
    SECTION("InstanceHandle creation and comparison") {
        InstanceHandle h1(0);
        InstanceHandle h2(0);
        InstanceHandle h3(1);

        REQUIRE(h1 == h2);
        REQUIRE(h1 != h3);
    }

    SECTION("Invalid InstanceHandle") {
        InstanceHandle invalid = InstanceHandle::INVALID;
        REQUIRE(invalid.is_invalid());

        InstanceHandle valid(5);
        REQUIRE_FALSE(valid.is_invalid());
    }
}

TEST_CASE("MeshInstance structure", "[renderer][world]") {
    SECTION("Create mesh instance with transform") {
        MeshInstance instance;
        instance.mesh = MeshHandle(1);
        instance.transform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        instance.prev_transform = instance.transform;

        REQUIRE(instance.mesh.id == 1);

        // Verify transform
        glm::vec4 origin(0.0f, 0.0f, 0.0f, 1.0f);
        glm::vec4 transformed = instance.transform * origin;

        REQUIRE_THAT(transformed.x, Catch::Matchers::WithinRel(1.0f, 0.001f));
        REQUIRE_THAT(transformed.y, Catch::Matchers::WithinRel(2.0f, 0.001f));
        REQUIRE_THAT(transformed.z, Catch::Matchers::WithinRel(3.0f, 0.001f));
    }

    SECTION("Instance with dynamic parameters") {
        MeshInstance instance;
        instance.mesh = MeshHandle(2);
        instance.dynamic_parameters = glm::vec4(1.0f, 0.5f, 0.25f, 1.0f);

        REQUIRE_THAT(instance.dynamic_parameters.x, Catch::Matchers::WithinRel(1.0f, 0.001f));
        REQUIRE_THAT(instance.dynamic_parameters.y, Catch::Matchers::WithinRel(0.5f, 0.001f));
        REQUIRE_THAT(instance.dynamic_parameters.z, Catch::Matchers::WithinRel(0.25f, 0.001f));
        REQUIRE_THAT(instance.dynamic_parameters.w, Catch::Matchers::WithinRel(1.0f, 0.001f));
    }
}

TEST_CASE("GbufferDepth structure", "[renderer][world]") {
    SECTION("Create GbufferDepth") {
        using namespace tekki::render_graph;

        Handle<tekki::vulkan::Image> normal(1);
        Handle<tekki::vulkan::Image> gbuffer(2);
        Handle<tekki::vulkan::Image> depth(3);

        GbufferDepth gbuffer_depth(normal, gbuffer, depth);

        REQUIRE(gbuffer_depth.normal == normal);
        REQUIRE(gbuffer_depth.gbuffer == gbuffer);
        REQUIRE(gbuffer_depth.depth == depth);
    }
}

TEST_CASE("ExposureState structure", "[renderer][world]") {
    SECTION("Default exposure state") {
        ExposureState exposure;

        REQUIRE_THAT(exposure.pre_mult, Catch::Matchers::WithinRel(1.0f, 0.001f));
        REQUIRE_THAT(exposure.pre_mult_prev, Catch::Matchers::WithinRel(1.0f, 0.001f));
        REQUIRE_THAT(exposure.pre_mult_delta, Catch::Matchers::WithinRel(1.0f, 0.001f));
        REQUIRE_THAT(exposure.post_mult, Catch::Matchers::WithinRel(1.0f, 0.001f));
    }

    SECTION("Exposure state update") {
        ExposureState exposure;

        exposure.pre_mult = 2.0f;
        exposure.pre_mult_prev = 1.5f;
        exposure.pre_mult_delta = exposure.pre_mult / exposure.pre_mult_prev;

        REQUIRE_THAT(exposure.pre_mult_delta, Catch::Matchers::WithinRel(2.0f / 1.5f, 0.001f));
    }
}

TEST_CASE("DynamicExposureState operations", "[renderer][world]") {
    SECTION("Default dynamic exposure") {
        DynamicExposureState dyn_exposure;

        REQUIRE_THAT(dyn_exposure.histogram_clipping.x, Catch::Matchers::WithinRel(0.0f, 0.001f));
        REQUIRE_THAT(dyn_exposure.histogram_clipping.y, Catch::Matchers::WithinRel(1.0f, 0.001f));
    }

    SECTION("Update exposure smoothing") {
        DynamicExposureState dyn_exposure;

        float luminance = 2.0f;
        float dt = 1.0f / 60.0f; // 60 FPS

        dyn_exposure.update(luminance, dt);

        REQUIRE_THAT(dyn_exposure.ev_smoothed(), Catch::Matchers::WithinAbs(luminance, 1.0f));
    }
}

TEST_CASE("RenderDebugMode enumeration", "[renderer][world]") {
    SECTION("Debug mode values") {
        RenderDebugMode mode1 = RenderDebugMode::None;
        RenderDebugMode mode2 = RenderDebugMode::WorldRadianceCache;

        REQUIRE(mode1 != mode2);
    }
}

TEST_CASE("RenderMode enumeration", "[renderer][world]") {
    SECTION("Render mode values") {
        RenderMode standard = RenderMode::Standard;
        RenderMode reference = RenderMode::Reference;

        REQUIRE(standard != reference);
    }

    SECTION("Render mode indexing") {
        // Verify enum can be used for array indexing
        std::array<int, 2> mode_data;
        mode_data[static_cast<size_t>(RenderMode::Standard)] = 100;
        mode_data[static_cast<size_t>(RenderMode::Reference)] = 200;

        REQUIRE(mode_data[0] == 100);
        REQUIRE(mode_data[1] == 200);
    }
}

TEST_CASE("BindlessImageHandle operations", "[renderer][world]") {
    SECTION("Create and compare bindless handles") {
        BindlessImageHandle h1(10);
        BindlessImageHandle h2(10);
        BindlessImageHandle h3(20);

        REQUIRE(h1 == h2);
        REQUIRE(h1 != h3);
    }

    SECTION("Invalid bindless handle") {
        BindlessImageHandle invalid = BindlessImageHandle::INVALID;
        REQUIRE(invalid.is_invalid());
    }
}
