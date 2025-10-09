#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <tekki/core/mmap.h>
#include <fstream>
#include <filesystem>

using namespace tekki::core;

TEST_CASE("Memory mapped file operations", "[core][mmap]") {
    // Create a temporary test file
    const std::string test_file_path = "test_mmap_file.bin";
    const std::string test_data = "Hello, memory-mapped world!";

    // Write test data to file
    {
        std::ofstream ofs(test_file_path, std::ios::binary);
        ofs.write(test_data.c_str(), test_data.size());
    }

    SECTION("Open and read memory-mapped file") {
        MemoryMappedFile mmap;
        REQUIRE(mmap.open(test_file_path));
        REQUIRE(mmap.data() != nullptr);
        REQUIRE(mmap.size() == test_data.size());

        // Verify content
        const char* mapped_data = static_cast<const char*>(mmap.data());
        std::string read_data(mapped_data, mmap.size());
        REQUIRE(read_data == test_data);

        mmap.close();
        REQUIRE(mmap.data() == nullptr);
        REQUIRE(mmap.size() == 0);
    }

    SECTION("Open non-existent file should fail") {
        MemoryMappedFile mmap;
        REQUIRE_FALSE(mmap.open("non_existent_file.bin"));
    }

    SECTION("Type-safe access") {
        // Create a file with structured data
        struct TestStruct {
            int32_t value1;
            float value2;
            int32_t value3;
        };

        const std::string struct_file_path = "test_struct.bin";
        TestStruct test_struct{42, 3.14f, 100};

        {
            std::ofstream ofs(struct_file_path, std::ios::binary);
            ofs.write(reinterpret_cast<const char*>(&test_struct), sizeof(TestStruct));
        }

        MemoryMappedFile mmap;
        REQUIRE(mmap.open(struct_file_path));

        const TestStruct* mapped_struct = mmap.as<TestStruct>();
        REQUIRE(mapped_struct != nullptr);
        REQUIRE(mapped_struct->value1 == 42);
        REQUIRE_THAT(mapped_struct->value2, Catch::Matchers::WithinRel(3.14f, 0.001f));
        REQUIRE(mapped_struct->value3 == 100);

        mmap.close();
        std::filesystem::remove(struct_file_path);
    }

    SECTION("RAII semantics") {
        {
            MemoryMappedFile mmap;
            mmap.open(test_file_path);
            REQUIRE(mmap.data() != nullptr);
            // Destructor should clean up automatically
        }

        // File should still exist
        REQUIRE(std::filesystem::exists(test_file_path));
    }

    // Cleanup
    std::filesystem::remove(test_file_path);
}

TEST_CASE("Memory mapped file error handling", "[core][mmap]") {
    SECTION("Access too small file with large type") {
        const std::string small_file = "small_file.bin";
        {
            std::ofstream ofs(small_file, std::ios::binary);
            ofs.write("AB", 2); // Only 2 bytes
        }

        MemoryMappedFile mmap;
        REQUIRE(mmap.open(small_file));

        struct LargeStruct {
            int64_t data[100];
        };

        // Should throw because file is too small
        REQUIRE_THROWS_AS(mmap.as<LargeStruct>(), std::runtime_error);

        mmap.close();
        std::filesystem::remove(small_file);
    }
}
