#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <tekki/renderer/half_res.h>

using namespace tekki::renderer;

TEST_CASE("HalfRes utility functions", "[renderer][half_res]") {
    SECTION("Halve extent calculation") {
        std::array<uint32_t, 2> full_res = {1920, 1080};
        std::array<uint32_t, 2> half_res = half_res_extent(full_res);

        REQUIRE(half_res[0] == 960);
        REQUIRE(half_res[1] == 540);
    }

    SECTION("Halve odd dimensions") {
        std::array<uint32_t, 2> full_res = {1921, 1081};
        std::array<uint32_t, 2> half_res = half_res_extent(full_res);

        // Should round up
        REQUIRE(half_res[0] == 961);
        REQUIRE(half_res[1] == 541);
    }

    SECTION("Minimum half resolution") {
        std::array<uint32_t, 2> small_res = {2, 2};
        std::array<uint32_t, 2> half_res = half_res_extent(small_res);

        REQUIRE(half_res[0] == 1);
        REQUIRE(half_res[1] == 1);
    }

    SECTION("Large resolution halving") {
        std::array<uint32_t, 2> large_res = {3840, 2160}; // 4K
        std::array<uint32_t, 2> half_res = half_res_extent(large_res);

        REQUIRE(half_res[0] == 1920);
        REQUIRE(half_res[1] == 1080);
    }
}

TEST_CASE("Quarter resolution calculations", "[renderer][half_res]") {
    SECTION("Quarter extent from full resolution") {
        std::array<uint32_t, 2> full_res = {1920, 1080};
        std::array<uint32_t, 2> half_res = half_res_extent(full_res);
        std::array<uint32_t, 2> quarter_res = half_res_extent(half_res);

        REQUIRE(quarter_res[0] == 480);
        REQUIRE(quarter_res[1] == 270);
    }
}

TEST_CASE("HalfRes filter operations", "[renderer][half_res]") {
    SECTION("HalfResFilterMode enumeration") {
        HalfResFilterMode min_filter = HalfResFilterMode::Min;
        HalfResFilterMode max_filter = HalfResFilterMode::Max;
        HalfResFilterMode average_filter = HalfResFilterMode::Average;

        REQUIRE(min_filter != max_filter);
        REQUIRE(max_filter != average_filter);
        REQUIRE(min_filter != average_filter);
    }
}
