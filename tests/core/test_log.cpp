#include <catch2/catch_test_macros.hpp>
#include <tekki/core/log.h>

TEST_CASE("Logging system initialization", "[core][log]") {
    SECTION("Initialize logger") {
        // Logger should initialize without throwing
        REQUIRE_NOTHROW(tekki::core::init_logger());
    }

    SECTION("Log messages at different levels") {
        tekki::core::init_logger();

        // These should not throw
        REQUIRE_NOTHROW(TEKKI_LOG_TRACE("Trace message"));
        REQUIRE_NOTHROW(TEKKI_LOG_DEBUG("Debug message"));
        REQUIRE_NOTHROW(TEKKI_LOG_INFO("Info message"));
        REQUIRE_NOTHROW(TEKKI_LOG_WARN("Warning message"));
        REQUIRE_NOTHROW(TEKKI_LOG_ERROR("Error message"));
    }
}

TEST_CASE("Logging with parameters", "[core][log]") {
    tekki::core::init_logger();

    SECTION("Format strings work correctly") {
        int value = 42;
        std::string text = "test";

        REQUIRE_NOTHROW(TEKKI_LOG_INFO("Integer: {}", value));
        REQUIRE_NOTHROW(TEKKI_LOG_INFO("String: {}", text));
        REQUIRE_NOTHROW(TEKKI_LOG_INFO("Multiple: {} and {}", value, text));
    }
}
