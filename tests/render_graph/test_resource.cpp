#include <catch2/catch_test_macros.hpp>
#include <tekki/render_graph/resource.h>
#include <vulkan/vulkan.hpp>

using namespace tekki::render_graph;

TEST_CASE("Resource handle system", "[render_graph][resource]") {
    SECTION("Handle creation and comparison") {
        Handle<vulkan::Image> handle1(0);
        Handle<vulkan::Image> handle2(0);
        Handle<vulkan::Image> handle3(1);

        REQUIRE(handle1 == handle2);
        REQUIRE(handle1 != handle3);
    }

    SECTION("Invalid handle") {
        Handle<vulkan::Image> invalid_handle = Handle<vulkan::Image>::INVALID;
        REQUIRE(invalid_handle.is_invalid());

        Handle<vulkan::Image> valid_handle(0);
        REQUIRE_FALSE(valid_handle.is_invalid());
    }

    SECTION("Handle ID access") {
        Handle<vulkan::Image> handle(42);
        REQUIRE(handle.id() == 42);
    }
}

TEST_CASE("Exported resource handles", "[render_graph][resource]") {
    SECTION("ExportedHandle creation") {
        ExportedHandle<vulkan::Image> exported(10);
        REQUIRE(exported.id() == 10);
    }

    SECTION("ExportedHandle comparison") {
        ExportedHandle<vulkan::Image> h1(5);
        ExportedHandle<vulkan::Image> h2(5);
        ExportedHandle<vulkan::Image> h3(6);

        REQUIRE(h1 == h2);
        REQUIRE(h1 != h3);
    }
}

TEST_CASE("Resource views", "[render_graph][resource]") {
    SECTION("GpuSrv view type") {
        // SRV is a read-only shader resource view
        Handle<vulkan::Image> image_handle(0);
        Ref<vulkan::Image, GpuSrv> srv_ref(image_handle);

        REQUIRE(srv_ref.handle() == image_handle);
    }

    SECTION("GpuUav view type") {
        // UAV is a read-write unordered access view
        Handle<vulkan::Image> image_handle(1);
        Ref<vulkan::Image, GpuUav> uav_ref(image_handle);

        REQUIRE(uav_ref.handle() == image_handle);
    }

    SECTION("GpuRt view type") {
        // RT is a render target view
        Handle<vulkan::Image> image_handle(2);
        Ref<vulkan::Image, GpuRt> rt_ref(image_handle);

        REQUIRE(rt_ref.handle() == image_handle);
    }
}

TEST_CASE("Buffer handle operations", "[render_graph][resource]") {
    SECTION("Buffer handles are independent from image handles") {
        Handle<vulkan::Buffer> buffer_handle(0);
        Handle<vulkan::Image> image_handle(0);

        // These are different types, cannot be compared
        // This test just verifies they can coexist with same ID
        REQUIRE(buffer_handle.id() == image_handle.id());
    }

    SECTION("Buffer reference creation") {
        Handle<vulkan::Buffer> buffer_handle(5);
        Ref<vulkan::Buffer, GpuSrv> buffer_srv(buffer_handle);

        REQUIRE(buffer_srv.handle() == buffer_handle);
    }
}
