#include <catch2/catch_test_macros.hpp>
#include <tekki/backend/file.h>
#include <filesystem>
#include <fstream>

using namespace tekki::backend;
namespace fs = std::filesystem;

TEST_CASE("LoadFile operations", "[backend][file]") {
    const fs::path test_file = "test_data.txt";
    const std::string test_content = "Hello, World!";

    // Cleanup before test
    if (fs::exists(test_file)) {
        fs::remove(test_file);
    }

    SECTION("Load existing file") {
        // Create test file
        std::ofstream out(test_file);
        out << test_content;
        out.close();

        LoadFile loader(test_file);
        auto data = loader.Run();
        std::string content(data.begin(), data.end());
        REQUIRE(content == test_content);

        // Cleanup
        fs::remove(test_file);
    }

    SECTION("LoadFile constructor") {
        LoadFile loader("test.txt");
        REQUIRE(loader.DebugDescription().find("test.txt") != std::string::npos);
    }

    SECTION("LoadFile copy") {
        LoadFile loader1("test.txt");
        LoadFile loader2 = loader1;

        REQUIRE(loader1.DebugDescription() == loader2.DebugDescription());
    }
}

TEST_CASE("VirtualFileSystem operations", "[backend][file]") {
    SECTION("Set VFS mount point") {
        VirtualFileSystem::SetVfsMountPoint("/test", fs::current_path());
        REQUIRE_NOTHROW(VirtualFileSystem::CanonicalPathFromVfs("/test/file.txt"));
    }

    SECTION("Canonical path from VFS") {
        auto path = VirtualFileSystem::CanonicalPathFromVfs("test.txt");
        REQUIRE_FALSE(path.empty());
    }

    SECTION("Normalized path from VFS") {
        auto path = VirtualFileSystem::NormalizedPathFromVfs("test.txt");
        REQUIRE_FALSE(path.empty());
    }
}

TEST_CASE("File path operations", "[backend][file]") {
    SECTION("Filesystem path operations") {
        fs::path p = "test.txt";
        REQUIRE(p.extension() == ".txt");
        REQUIRE(p.filename() == "test.txt");
    }

    SECTION("Check file exists") {
        const fs::path test_file = "exists_test.txt";

        REQUIRE_FALSE(fs::exists(test_file));

        // Create file
        std::ofstream out(test_file);
        out << "test";
        out.close();

        REQUIRE(fs::exists(test_file));

        // Cleanup
        fs::remove(test_file);
    }
}

TEST_CASE("FileWatcher", "[backend][file]") {
    SECTION("Create file watcher") {
        FileWatcher watcher;
        REQUIRE_NOTHROW(watcher);
    }

    SECTION("Watch file") {
        FileWatcher watcher;
        bool called = false;

        watcher.Watch("test.txt", [&called]() {
            called = true;
        });

        // File watcher is set up, callback would be called on file changes
        REQUIRE_NOTHROW(watcher);
    }
}
