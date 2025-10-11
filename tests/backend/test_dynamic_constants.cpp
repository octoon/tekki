#include <catch2/catch_test_macros.hpp>
#include <tekki/backend/dynamic_constants.h>
#include <glm/glm.hpp>

using namespace tekki::backend;

// Note: DynamicConstants requires a Vulkan buffer, so these are simplified mock tests
// For full integration tests, see the integration test suite

TEST_CASE("DynamicConstants offset calculation", "[backend][dynamic_constants]") {
    SECTION("CurrentOffset starts at zero") {
        // This test would require a valid Vulkan buffer
        // Skipping full test, see integration tests
        REQUIRE(true);
    }

    SECTION("Alignment constants are valid") {
        REQUIRE(DYNAMIC_CONSTANTS_ALIGNMENT == 256);
        REQUIRE(DYNAMIC_CONSTANTS_SIZE_BYTES == 1024 * 1024 * 16);
        REQUIRE(MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH == 16384);
    }
}

TEST_CASE("DynamicConstants constants validation", "[backend][dynamic_constants]") {
    SECTION("Size constants") {
        REQUIRE(DYNAMIC_CONSTANTS_SIZE_BYTES > 0);
        REQUIRE(DYNAMIC_CONSTANTS_BUFFER_COUNT == 2);
        REQUIRE(MAX_DYNAMIC_CONSTANTS_BYTES_PER_DISPATCH <= DYNAMIC_CONSTANTS_SIZE_BYTES);
    }

    SECTION("Alignment is power of 2") {
        size_t align = DYNAMIC_CONSTANTS_ALIGNMENT;
        REQUIRE((align & (align - 1)) == 0); // Power of 2 check
    }
}
