#include <catch2/catch_test_macros.hpp>
#include <tekki/core/result.h>

using namespace tekki::core;

TEST_CASE("Result basic operations", "[core][result]") {
    SECTION("Ok result") {
        Result<int> result = Result<int>::Ok(42);

        REQUIRE(result.IsOk());
        REQUIRE_FALSE(result.IsErr());
        REQUIRE(result);
        REQUIRE(result.Value() == 42);
    }

    SECTION("Err result") {
        Result<int> result = Result<int>::Err("Test error");

        REQUIRE_FALSE(result.IsOk());
        REQUIRE(result.IsErr());
        REQUIRE_FALSE(result);
        REQUIRE(result.GetError().Message == "Test error");
    }

    SECTION("Ok from value") {
        Result<int> result(42);
        REQUIRE(result.IsOk());
        REQUIRE(result.Value() == 42);
    }

    SECTION("Err from error") {
        Result<int> result(MakeError("Error message"));
        REQUIRE(result.IsErr());
        REQUIRE(result.GetError().Message == "Error message");
    }
}

TEST_CASE("Result value access", "[core][result]") {
    SECTION("Value() on Ok result") {
        Result<int> result = Result<int>::Ok(100);
        REQUIRE(result.Value() == 100);
    }

    SECTION("Value() on Err result throws") {
        Result<int> result = Result<int>::Err("Error");
        REQUIRE_THROWS_AS(result.Value(), std::runtime_error);
    }

    SECTION("ValueOr with Ok result") {
        Result<int> result = Result<int>::Ok(42);
        REQUIRE(result.ValueOr(999) == 42);
    }

    SECTION("ValueOr with Err result") {
        Result<int> result = Result<int>::Err("Error");
        REQUIRE(result.ValueOr(999) == 999);
    }
}

TEST_CASE("Result unwrap and expect", "[core][result]") {
    SECTION("Unwrap on Ok result") {
        Result<int> result = Result<int>::Ok(42);
        REQUIRE(std::move(result).Unwrap() == 42);
    }

    SECTION("Unwrap on Err result throws") {
        Result<int> result = Result<int>::Err("Error");
        REQUIRE_THROWS_AS(std::move(result).Unwrap(), std::runtime_error);
    }

    SECTION("Expect on Ok result") {
        Result<int> result = Result<int>::Ok(42);
        REQUIRE(std::move(result).Expect("Should not fail") == 42);
    }

    SECTION("Expect on Err result throws with custom message") {
        Result<int> result = Result<int>::Err("Original error");
        REQUIRE_THROWS_WITH(
            std::move(result).Expect("Custom message"),
            Catch::Matchers::ContainsSubstring("Custom message")
        );
    }
}

TEST_CASE("Result map transformation", "[core][result]") {
    SECTION("Map on Ok result") {
        Result<int> result = Result<int>::Ok(10);
        auto mapped = result.Map([](int x) { return x * 2; });

        REQUIRE(mapped.IsOk());
        REQUIRE(mapped.Value() == 20);
    }

    SECTION("Map on Err result") {
        Result<int> result = Result<int>::Err("Error");
        auto mapped = result.Map([](int x) { return x * 2; });

        REQUIRE(mapped.IsErr());
        REQUIRE(mapped.GetError().Message == "Error");
    }

    SECTION("Map changing type") {
        Result<int> result = Result<int>::Ok(42);
        auto mapped = result.Map([](int x) { return std::to_string(x); });

        REQUIRE(mapped.IsOk());
        REQUIRE(mapped.Value() == "42");
    }
}

TEST_CASE("Result context addition", "[core][result]") {
    SECTION("WithContext on Err result") {
        Result<int> result = Result<int>::Err("Original error");
        auto with_context = std::move(result).WithContext("Operation failed");

        REQUIRE(with_context.IsErr());
        REQUIRE(with_context.GetError().Message == "Operation failed: Original error");
    }

    SECTION("WithContext on Ok result") {
        Result<int> result = Result<int>::Ok(42);
        auto with_context = std::move(result).WithContext("Context");

        REQUIRE(with_context.IsOk());
        REQUIRE(with_context.Value() == 42);
    }

    SECTION("Multiple context layers") {
        Result<int> result = Result<int>::Err("Base error");
        auto result2 = std::move(result).WithContext("Layer 1");
        auto result3 = std::move(result2).WithContext("Layer 2");

        REQUIRE(result3.GetError().Message == "Layer 2: Layer 1: Base error");
    }
}

TEST_CASE("Result<void> specialization", "[core][result]") {
    SECTION("Ok void result") {
        Result<void> result = Result<void>::Ok();

        REQUIRE(result.IsOk());
        REQUIRE_FALSE(result.IsErr());
        REQUIRE(result);
    }

    SECTION("Err void result") {
        Result<void> result = Result<void>::Err("Error");

        REQUIRE_FALSE(result.IsOk());
        REQUIRE(result.IsErr());
        REQUIRE(result.GetError().Message == "Error");
    }

    SECTION("Unwrap on Ok void result") {
        Result<void> result = Result<void>::Ok();
        REQUIRE_NOTHROW(result.Unwrap());
    }

    SECTION("Unwrap on Err void result throws") {
        Result<void> result = Result<void>::Err("Error");
        REQUIRE_THROWS_AS(result.Unwrap(), std::runtime_error);
    }

    SECTION("WithContext on void result") {
        Result<void> result = Result<void>::Err("Error");
        auto with_context = std::move(result).WithContext("Context");

        REQUIRE(with_context.GetError().Message == "Context: Error");
    }
}

TEST_CASE("Error class operations", "[core][result]") {
    SECTION("Error construction") {
        Error err("Test error");
        REQUIRE(err.Message == "Test error");
        REQUIRE(err.What() == std::string("Test error"));
    }

    SECTION("Error WithContext") {
        Error err("Original");
        Error with_context = err.WithContext("Context");

        REQUIRE(with_context.Message == "Context: Original");
        REQUIRE(err.Message == "Original"); // Original unchanged
    }

    SECTION("MakeError helper") {
        auto err = MakeError("Helper error");
        REQUIRE(err.Message == "Helper error");
    }
}

TEST_CASE("Result with complex types", "[core][result]") {
    SECTION("Result with std::string") {
        Result<std::string> result = Result<std::string>::Ok("Hello");
        REQUIRE(result.Value() == "Hello");
    }

    SECTION("Result with std::vector") {
        std::vector<int> vec = {1, 2, 3, 4, 5};
        Result<std::vector<int>> result = Result<std::vector<int>>::Ok(vec);

        REQUIRE(result.IsOk());
        REQUIRE(result.Value().size() == 5);
        REQUIRE(result.Value()[0] == 1);
    }

    SECTION("Result with std::unique_ptr") {
        auto ptr = std::make_unique<int>(42);
        Result<std::unique_ptr<int>> result = Result<std::unique_ptr<int>>::Ok(std::move(ptr));

        REQUIRE(result.IsOk());
        REQUIRE(*result.Value() == 42);
    }
}
