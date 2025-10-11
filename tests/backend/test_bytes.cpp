#include <catch2/catch_test_macros.hpp>
#include <tekki/backend/bytes.h>
#include <glm/glm.hpp>

using namespace tekki::backend;

TEST_CASE("IntoByteVec basic types", "[backend][bytes]") {
    SECTION("Convert vector of ints") {
        std::vector<int> v = {1, 2, 3, 4};
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.size() == v.size() * sizeof(int));
        REQUIRE_FALSE(bytes.empty());
    }

    SECTION("Convert vector of floats") {
        std::vector<float> v = {1.0f, 2.0f, 3.0f};
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.size() == v.size() * sizeof(float));
    }

    SECTION("Convert empty vector") {
        std::vector<int> v;
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.empty());
    }

    SECTION("Convert vector of uint8_t") {
        std::vector<uint8_t> v = {0x01, 0x02, 0x03, 0x04};
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.size() == 4);
        REQUIRE(bytes[0] == 0x01);
        REQUIRE(bytes[1] == 0x02);
        REQUIRE(bytes[2] == 0x03);
        REQUIRE(bytes[3] == 0x04);
    }

    SECTION("Convert vector of doubles") {
        std::vector<double> v = {1.5, 2.5, 3.5};
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.size() == v.size() * sizeof(double));
    }
}

TEST_CASE("IntoByteVec with GLM types", "[backend][bytes]") {
    SECTION("Convert vector of glm::vec3") {
        std::vector<glm::vec3> v = {
            glm::vec3(1.0f, 2.0f, 3.0f),
            glm::vec3(4.0f, 5.0f, 6.0f)
        };
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.size() == v.size() * sizeof(glm::vec3));
    }

    SECTION("Convert vector of glm::vec4") {
        std::vector<glm::vec4> v = {
            glm::vec4(1.0f, 2.0f, 3.0f, 4.0f),
            glm::vec4(5.0f, 6.0f, 7.0f, 8.0f)
        };
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.size() == v.size() * sizeof(glm::vec4));
    }

    SECTION("Convert vector of glm::mat4") {
        std::vector<glm::mat4> v = {glm::mat4(1.0f), glm::mat4(2.0f)};
        auto bytes = IntoByteVec(v);

        REQUIRE(bytes.size() == v.size() * sizeof(glm::mat4));
    }
}

TEST_CASE("AsByteSlice basic types", "[backend][bytes]") {
    SECTION("Get byte slice of int") {
        int value = 42;
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(int));

        // Verify the bytes represent the value
        int reconstructed = *reinterpret_cast<const int*>(ptr);
        REQUIRE(reconstructed == 42);
    }

    SECTION("Get byte slice of float") {
        float value = 3.14f;
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(float));

        float reconstructed = *reinterpret_cast<const float*>(ptr);
        REQUIRE(reconstructed == 3.14f);
    }

    SECTION("Get byte slice of double") {
        double value = 2.718281828;
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(double));
    }

    SECTION("Get byte slice of struct") {
        struct TestStruct {
            int a;
            float b;
            double c;
        };

        TestStruct value{42, 3.14f, 2.718};
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(TestStruct));

        // Verify reconstruction
        const TestStruct* reconstructed = reinterpret_cast<const TestStruct*>(ptr);
        REQUIRE(reconstructed->a == 42);
        REQUIRE(reconstructed->b == 3.14f);
        REQUIRE(reconstructed->c == 2.718);
    }
}

TEST_CASE("AsByteSlice with GLM types", "[backend][bytes]") {
    SECTION("Get byte slice of glm::vec3") {
        glm::vec3 value(1.0f, 2.0f, 3.0f);
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(glm::vec3));

        const glm::vec3* reconstructed = reinterpret_cast<const glm::vec3*>(ptr);
        REQUIRE(reconstructed->x == 1.0f);
        REQUIRE(reconstructed->y == 2.0f);
        REQUIRE(reconstructed->z == 3.0f);
    }

    SECTION("Get byte slice of glm::vec4") {
        glm::vec4 value(1.0f, 2.0f, 3.0f, 4.0f);
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(glm::vec4));
    }

    SECTION("Get byte slice of glm::mat4") {
        glm::mat4 value = glm::mat4(1.0f);
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(glm::mat4));
    }

    SECTION("Get byte slice of glm::ivec3") {
        glm::ivec3 value(10, 20, 30);
        auto [ptr, size] = AsByteSlice(value);

        REQUIRE(ptr != nullptr);
        REQUIRE(size == sizeof(glm::ivec3));

        const glm::ivec3* reconstructed = reinterpret_cast<const glm::ivec3*>(ptr);
        REQUIRE(reconstructed->x == 10);
        REQUIRE(reconstructed->y == 20);
        REQUIRE(reconstructed->z == 30);
    }
}

TEST_CASE("Byte conversion round-trip", "[backend][bytes]") {
    SECTION("Round-trip for integers") {
        std::vector<int> original = {1, 2, 3, 4, 5};
        auto bytes = IntoByteVec(original);

        // Reconstruct from bytes
        REQUIRE(bytes.size() == original.size() * sizeof(int));
        const int* reconstructed = reinterpret_cast<const int*>(bytes.data());

        for (size_t i = 0; i < original.size(); ++i) {
            REQUIRE(reconstructed[i] == original[i]);
        }
    }

    SECTION("Round-trip for floats") {
        std::vector<float> original = {1.1f, 2.2f, 3.3f, 4.4f};
        auto bytes = IntoByteVec(original);

        const float* reconstructed = reinterpret_cast<const float*>(bytes.data());
        for (size_t i = 0; i < original.size(); ++i) {
            REQUIRE(reconstructed[i] == original[i]);
        }
    }

    SECTION("Round-trip for glm::vec3") {
        std::vector<glm::vec3> original = {
            glm::vec3(1.0f, 2.0f, 3.0f),
            glm::vec3(4.0f, 5.0f, 6.0f),
            glm::vec3(7.0f, 8.0f, 9.0f)
        };
        auto bytes = IntoByteVec(original);

        const glm::vec3* reconstructed = reinterpret_cast<const glm::vec3*>(bytes.data());
        for (size_t i = 0; i < original.size(); ++i) {
            REQUIRE(reconstructed[i].x == original[i].x);
            REQUIRE(reconstructed[i].y == original[i].y);
            REQUIRE(reconstructed[i].z == original[i].z);
        }
    }
}
