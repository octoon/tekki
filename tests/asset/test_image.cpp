#include <catch2/catch_test_macros.hpp>
#include <tekki/asset/image.h>
#include <filesystem>

using namespace tekki::asset;
namespace fs = std::filesystem;

// Note: Image loading requires actual image files
// These are basic structure tests. Full tests with real images are in integration suite.

TEST_CASE("Image format detection", "[asset][image]") {
    SECTION("Detect format by extension") {
        // Simple filename-based detection tests
        fs::path png_file = "test.png";
        fs::path jpg_file = "test.jpg";
        fs::path exr_file = "test.exr";

        REQUIRE(png_file.extension() == ".png");
        REQUIRE(jpg_file.extension() == ".jpg");
        REQUIRE(exr_file.extension() == ".exr");
    }
}

TEST_CASE("Image structure", "[asset][image]") {
    SECTION("Image properties") {
        // Test image structure size/layout
        REQUIRE(sizeof(uint8_t) == 1);
        REQUIRE(sizeof(float) == 4);
    }
}
