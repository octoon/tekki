#include <catch2/catch_test_macros.hpp>
#include <tekki/render_graph/temporal.h>

using namespace tekki::render_graph;

TEST_CASE("PingPongTemporalResource basic operations", "[render_graph][temporal]") {
    SECTION("Initial state") {
        PingPongTemporalResource resource;

        REQUIRE(resource.current().is_invalid());
        REQUIRE(resource.history().is_invalid());
    }

    SECTION("Set current resource") {
        PingPongTemporalResource resource;
        Handle<vulkan::Image> current_handle(1);

        resource.set_current(current_handle);

        REQUIRE(resource.current() == current_handle);
        REQUIRE(resource.history().is_invalid());
    }

    SECTION("Advance creates history") {
        PingPongTemporalResource resource;
        Handle<vulkan::Image> frame1(1);
        Handle<vulkan::Image> frame2(2);

        resource.set_current(frame1);
        resource.advance();

        // After advance, current becomes history, and current is invalid
        REQUIRE(resource.history() == frame1);
        REQUIRE(resource.current().is_invalid());

        // Set new current
        resource.set_current(frame2);

        REQUIRE(resource.current() == frame2);
        REQUIRE(resource.history() == frame1);
    }

    SECTION("Multiple advances") {
        PingPongTemporalResource resource;
        Handle<vulkan::Image> frame1(1);
        Handle<vulkan::Image> frame2(2);
        Handle<vulkan::Image> frame3(3);

        resource.set_current(frame1);
        resource.advance();

        resource.set_current(frame2);
        resource.advance();

        REQUIRE(resource.history() == frame2);

        resource.set_current(frame3);
        REQUIRE(resource.current() == frame3);
        REQUIRE(resource.history() == frame2);
    }
}

TEST_CASE("TripleBufferTemporalResource basic operations", "[render_graph][temporal]") {
    SECTION("Initial state") {
        TripleBufferTemporalResource resource;

        REQUIRE(resource.current().is_invalid());
        REQUIRE(resource.history(0).is_invalid());
        REQUIRE(resource.history(1).is_invalid());
    }

    SECTION("Set current resource") {
        TripleBufferTemporalResource resource;
        Handle<vulkan::Image> current_handle(1);

        resource.set_current(current_handle);

        REQUIRE(resource.current() == current_handle);
        REQUIRE(resource.history(0).is_invalid());
        REQUIRE(resource.history(1).is_invalid());
    }

    SECTION("Advance creates history chain") {
        TripleBufferTemporalResource resource;
        Handle<vulkan::Image> frame1(1);
        Handle<vulkan::Image> frame2(2);
        Handle<vulkan::Image> frame3(3);

        // Frame 1
        resource.set_current(frame1);
        resource.advance();

        REQUIRE(resource.history(0) == frame1);
        REQUIRE(resource.history(1).is_invalid());

        // Frame 2
        resource.set_current(frame2);
        resource.advance();

        REQUIRE(resource.history(0) == frame2);
        REQUIRE(resource.history(1) == frame1);

        // Frame 3
        resource.set_current(frame3);

        REQUIRE(resource.current() == frame3);
        REQUIRE(resource.history(0) == frame2);
        REQUIRE(resource.history(1) == frame1);
    }

    SECTION("History wraps correctly") {
        TripleBufferTemporalResource resource;
        Handle<vulkan::Image> frame1(1);
        Handle<vulkan::Image> frame2(2);
        Handle<vulkan::Image> frame3(3);
        Handle<vulkan::Image> frame4(4);

        resource.set_current(frame1);
        resource.advance();

        resource.set_current(frame2);
        resource.advance();

        resource.set_current(frame3);
        resource.advance();

        resource.set_current(frame4);

        REQUIRE(resource.current() == frame4);
        REQUIRE(resource.history(0) == frame3);
        REQUIRE(resource.history(1) == frame2);
        // frame1 is now lost (only 2 frames of history)
    }
}

TEST_CASE("Temporal resource practical usage", "[render_graph][temporal]") {
    SECTION("TAA temporal accumulation pattern") {
        PingPongTemporalResource taa_resource;

        // Frame 0
        Handle<vulkan::Image> taa_frame0(100);
        taa_resource.set_current(taa_frame0);

        REQUIRE(taa_resource.history().is_invalid()); // No history yet
        taa_resource.advance();

        // Frame 1
        Handle<vulkan::Image> taa_frame1(101);
        taa_resource.set_current(taa_frame1);

        REQUIRE(taa_resource.current() == taa_frame1);
        REQUIRE(taa_resource.history() == taa_frame0); // Can blend with previous frame
        taa_resource.advance();

        // Frame 2
        Handle<vulkan::Image> taa_frame2(102);
        taa_resource.set_current(taa_frame2);

        REQUIRE(taa_resource.current() == taa_frame2);
        REQUIRE(taa_resource.history() == taa_frame1);
    }

    SECTION("Motion vector reprojection pattern") {
        TripleBufferTemporalResource motion_resource;

        // Need 2 frames of history for advanced motion vector techniques
        Handle<vulkan::Image> mv_frame0(200);
        Handle<vulkan::Image> mv_frame1(201);
        Handle<vulkan::Image> mv_frame2(202);

        motion_resource.set_current(mv_frame0);
        motion_resource.advance();

        motion_resource.set_current(mv_frame1);
        motion_resource.advance();

        motion_resource.set_current(mv_frame2);

        // Can access 2 frames of history
        REQUIRE(motion_resource.current() == mv_frame2);
        REQUIRE(motion_resource.history(0) == mv_frame1);
        REQUIRE(motion_resource.history(1) == mv_frame0);
    }
}
