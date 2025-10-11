#include <catch2/catch_test_macros.hpp>
#include <tekki/backend/chunky_list.h>
#include <string>

using namespace tekki::backend;

TEST_CASE("TempList basic operations", "[backend][chunky_list]") {
    SECTION("Create empty list") {
        TempList<int> list;
        REQUIRE_NOTHROW(list);
    }

    SECTION("Add single item") {
        TempList<int> list;
        const int& ref = list.Add(42);

        REQUIRE(ref == 42);
    }

    SECTION("Add multiple items") {
        TempList<int> list;
        const int& ref1 = list.Add(1);
        const int& ref2 = list.Add(2);
        const int& ref3 = list.Add(3);

        REQUIRE(ref1 == 1);
        REQUIRE(ref2 == 2);
        REQUIRE(ref3 == 3);
    }

    SECTION("Add items by const reference") {
        TempList<std::string> list;
        std::string s1 = "Hello";
        std::string s2 = "World";

        const std::string& ref1 = list.Add(s1);
        const std::string& ref2 = list.Add(s2);

        REQUIRE(ref1 == "Hello");
        REQUIRE(ref2 == "World");
    }

    SECTION("Add items by move") {
        TempList<std::string> list;
        std::string s1 = "Hello";
        std::string s2 = "World";

        const std::string& ref1 = list.Add(std::move(s1));
        const std::string& ref2 = list.Add(std::move(s2));

        REQUIRE(ref1 == "Hello");
        REQUIRE(ref2 == "World");
    }
}

TEST_CASE("TempList with complex types", "[backend][chunky_list]") {
    SECTION("List of std::string") {
        TempList<std::string> list;
        const std::string& ref = list.Add("Test string");

        REQUIRE(ref == "Test string");
    }

    SECTION("List of std::vector") {
        TempList<std::vector<int>> list;
        std::vector<int> vec = {1, 2, 3, 4, 5};
        const std::vector<int>& ref = list.Add(vec);

        REQUIRE(ref.size() == 5);
        REQUIRE(ref[0] == 1);
        REQUIRE(ref[4] == 5);
    }

    SECTION("List of custom struct") {
        struct TestStruct {
            int id;
            std::string name;
            float value;
        };

        TempList<TestStruct> list;
        TestStruct item{42, "Test", 3.14f};
        const TestStruct& ref = list.Add(item);

        REQUIRE(ref.id == 42);
        REQUIRE(ref.name == "Test");
        REQUIRE(ref.value == 3.14f);
    }
}

TEST_CASE("TempList reference stability", "[backend][chunky_list]") {
    SECTION("References remain valid after adding more items") {
        TempList<int> list;

        // Add first item and keep reference
        const int& ref1 = list.Add(100);
        REQUIRE(ref1 == 100);

        // Add more items
        list.Add(200);
        list.Add(300);
        list.Add(400);

        // Original reference should still be valid
        REQUIRE(ref1 == 100);
    }

    SECTION("Multiple references remain valid") {
        TempList<std::string> list;

        const std::string& ref1 = list.Add("First");
        const std::string& ref2 = list.Add("Second");
        const std::string& ref3 = list.Add("Third");

        // Add more items
        list.Add("Fourth");
        list.Add("Fifth");

        // All references should still be valid
        REQUIRE(ref1 == "First");
        REQUIRE(ref2 == "Second");
        REQUIRE(ref3 == "Third");
    }
}

TEST_CASE("TempList capacity management", "[backend][chunky_list]") {
    SECTION("List expands when capacity is reached") {
        TempList<int> list;

        // Add many items to force expansion (default capacity is 8)
        std::vector<int> values;
        for (int i = 0; i < 20; ++i) {
            const int& ref = list.Add(i);
            values.push_back(ref);
        }

        // Verify all values are correct
        for (int i = 0; i < 20; ++i) {
            REQUIRE(values[i] == i);
        }
    }

    SECTION("References stable across chunk boundaries") {
        TempList<int> list;

        // Store references to first 10 items
        std::vector<const int*> refs;
        for (int i = 0; i < 10; ++i) {
            refs.push_back(&list.Add(i * 10));
        }

        // Add many more items to create new chunks
        for (int i = 10; i < 50; ++i) {
            list.Add(i * 10);
        }

        // Verify original references are still valid
        for (size_t i = 0; i < refs.size(); ++i) {
            REQUIRE(*refs[i] == static_cast<int>(i) * 10);
        }
    }
}

TEST_CASE("TempList edge cases", "[backend][chunky_list]") {
    SECTION("Add zero-sized type") {
        struct Empty {};
        TempList<Empty> list;

        Empty e;
        REQUIRE_NOTHROW(list.Add(e));
    }

    SECTION("Add items with same value") {
        TempList<int> list;

        const int& ref1 = list.Add(42);
        const int& ref2 = list.Add(42);
        const int& ref3 = list.Add(42);

        REQUIRE(ref1 == 42);
        REQUIRE(ref2 == 42);
        REQUIRE(ref3 == 42);

        // Verify they are different objects
        REQUIRE(&ref1 != &ref2);
        REQUIRE(&ref2 != &ref3);
    }
}
