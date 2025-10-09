#include <catch2/catch_test_macros.hpp>
#include <tekki/asset/image.h>
#include <filesystem>
#include <fstream>

using namespace tekki::asset;

TEST_CASE("Image format detection", "[asset][image]") {
    SECTION("Detect PNG format") {
        const std::string png_path = "test.png";
        ImageFormat format = detect_image_format(png_path);
        REQUIRE(format == ImageFormat::PNG);
    }

    SECTION("Detect JPEG format") {
        const std::string jpg_path = "test.jpg";
        ImageFormat format = detect_image_format(jpg_path);
        REQUIRE(format == ImageFormat::JPEG);

        const std::string jpeg_path = "test.jpeg";
        format = detect_image_format(jpeg_path);
        REQUIRE(format == ImageFormat::JPEG);
    }

    SECTION("Detect HDR format") {
        const std::string hdr_path = "test.hdr";
        ImageFormat format = detect_image_format(hdr_path);
        REQUIRE(format == ImageFormat::HDR);
    }

    SECTION("Detect EXR format") {
        const std::string exr_path = "test.exr";
        ImageFormat format = detect_image_format(exr_path);
        REQUIRE(format == ImageFormat::EXR);
    }

    SECTION("Detect DDS format") {
        const std::string dds_path = "test.dds";
        ImageFormat format = detect_image_format(dds_path);
        REQUIRE(format == ImageFormat::DDS);
    }

    SECTION("Unknown format") {
        const std::string unknown_path = "test.xyz";
        ImageFormat format = detect_image_format(unknown_path);
        REQUIRE(format == ImageFormat::Unknown);
    }
}

TEST_CASE("Image data structure", "[asset][image]") {
    SECTION("Create empty image") {
        Image img;
        REQUIRE(img.width == 0);
        REQUIRE(img.height == 0);
        REQUIRE(img.channels == 0);
        REQUIRE(img.data.empty());
    }

    SECTION("Create image with data") {
        Image img;
        img.width = 256;
        img.height = 256;
        img.channels = 4; // RGBA
        img.data.resize(256 * 256 * 4);

        REQUIRE(img.width == 256);
        REQUIRE(img.height == 256);
        REQUIRE(img.channels == 4);
        REQUIRE(img.data.size() == 256 * 256 * 4);
    }

    SECTION("Calculate mip levels") {
        Image img;
        img.width = 1024;
        img.height = 1024;

        // Mip levels = log2(max(width, height)) + 1
        uint32_t max_dim = std::max(img.width, img.height);
        uint32_t mip_levels = 0;
        while (max_dim > 0) {
            mip_levels++;
            max_dim >>= 1;
        }

        REQUIRE(mip_levels == 11); // log2(1024) + 1
    }
}

TEST_CASE("Image descriptor", "[asset][image]") {
    SECTION("Image descriptor basic properties") {
        ImageDesc desc;
        desc.width = 512;
        desc.height = 512;
        desc.format = ImageFormat::PNG;
        desc.is_srgb = true;

        REQUIRE(desc.width == 512);
        REQUIRE(desc.height == 512);
        REQUIRE(desc.format == ImageFormat::PNG);
        REQUIRE(desc.is_srgb == true);
    }
}

TEST_CASE("Swizzle operations", "[asset][image]") {
    SECTION("Channel swizzle identity") {
        Swizzle swizzle = Swizzle::RGBA;
        REQUIRE(swizzle == Swizzle::RGBA);
    }

    SECTION("Channel swizzle operations") {
        // Test that swizzle enum exists and can be assigned
        Swizzle rgb_to_bgr = Swizzle::BGRA;
        Swizzle single_channel = Swizzle::RRR1;
        Swizzle normal_map = Swizzle::RRRG; // DXT5 normal map unswizzle

        REQUIRE(rgb_to_bgr != single_channel);
        REQUIRE(single_channel != normal_map);
    }
}

TEST_CASE("Image compression", "[asset][image]") {
    SECTION("BC5 compression format") {
        // BC5 is used for normal maps (2 channels)
        auto format = CompressionFormat::BC5;
        REQUIRE(format == CompressionFormat::BC5);
    }

    SECTION("BC7 compression format") {
        // BC7 is used for high-quality color textures
        auto format = CompressionFormat::BC7;
        REQUIRE(format == CompressionFormat::BC7);
    }

    SECTION("Uncompressed format") {
        auto format = CompressionFormat::None;
        REQUIRE(format == CompressionFormat::None);
    }
}

TEST_CASE("MipDesc structure", "[asset][image]") {
    SECTION("Mip descriptor properties") {
        MipDesc mip;
        mip.width = 128;
        mip.height = 128;
        mip.offset = 0;
        mip.size = 128 * 128 * 4;

        REQUIRE(mip.width == 128);
        REQUIRE(mip.height == 128);
        REQUIRE(mip.offset == 0);
        REQUIRE(mip.size == 128 * 128 * 4);
    }

    SECTION("Mip chain calculation") {
        std::vector<MipDesc> mip_chain;
        uint32_t width = 256;
        uint32_t height = 256;
        uint32_t offset = 0;

        while (width >= 1 && height >= 1) {
            MipDesc mip;
            mip.width = width;
            mip.height = height;
            mip.offset = offset;
            mip.size = width * height * 4; // RGBA

            mip_chain.push_back(mip);

            offset += mip.size;
            width /= 2;
            height /= 2;

            if (width == 0 || height == 0) break;
        }

        REQUIRE(mip_chain.size() == 9); // 256->128->64->32->16->8->4->2->1
        REQUIRE(mip_chain[0].width == 256);
        REQUIRE(mip_chain[8].width == 1);
    }
}
