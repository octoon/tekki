// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <filesystem>
#include <string>

#include <spdlog/common.h>

namespace tekki::config {

struct AppConfig {
    int window_width = 1280;
    int window_height = 720;
    std::string window_title = "tekki viewer";
    spdlog::level::level_enum log_level = spdlog::level::info;
    bool enable_validation = true;
    unsigned bootstrap_frames = 0; // 0 表示无限，直到窗口关闭
    std::filesystem::path config_path;
};

AppConfig load_from_file(const std::filesystem::path& path);

} // namespace tekki::config
