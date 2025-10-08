// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "core/config.h"

#include <fstream>
#include <iostream>
#include <cctype>

#include <nlohmann/json.hpp>

namespace tekki::config {
namespace {

spdlog::level::level_enum parse_log_level(const std::string& value) {
    const auto lowered = [&]() {
        std::string tmp = value;
        for (char& c : tmp) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return tmp;
    }();

    if (lowered == "trace") return spdlog::level::trace;
    if (lowered == "debug") return spdlog::level::debug;
    if (lowered == "warn" || lowered == "warning") return spdlog::level::warn;
    if (lowered == "error") return spdlog::level::err;
    if (lowered == "critical" || lowered == "fatal") return spdlog::level::critical;
    return spdlog::level::info;
}

} // namespace

AppConfig load_from_file(const std::filesystem::path& path) {
    AppConfig config{};
    config.config_path = path;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return config; // 使用默认值
    }

    try {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << path << "\n";
            return config;
        }

        nlohmann::json json;
        file >> json;

        if (auto window = json.find("window"); window != json.end()) {
            if (window->contains("width")) {
                config.window_width = static_cast<int>((*window)["width"].get<int>());
            }
            if (window->contains("height")) {
                config.window_height = static_cast<int>((*window)["height"].get<int>());
            }
            if (window->contains("title")) {
                config.window_title = (*window)["title"].get<std::string>();
            }
        }

        if (auto logging = json.find("logging"); logging != json.end()) {
            if (logging->contains("level")) {
                config.log_level = parse_log_level((*logging)["level"].get<std::string>());
            }
        }

        if (auto renderer = json.find("renderer"); renderer != json.end()) {
            if (renderer->contains("validation")) {
                config.enable_validation = (*renderer)["validation"].get<bool>();
            }
        }

        if (auto bootstrap = json.find("bootstrap"); bootstrap != json.end()) {
            if (bootstrap->contains("frame_limit")) {
                config.bootstrap_frames = (*bootstrap)["frame_limit"].get<unsigned>();
            }
        }
    } catch (const std::exception& err) {
        std::cerr << "Error parsing config file: " << path << " -> " << err.what() << "\n";
    }

    return config;
}

} // namespace tekki::config
