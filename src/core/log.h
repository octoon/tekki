// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <spdlog/spdlog.h>
#include <memory>

namespace tekki::log {

/**
 * @brief Initialize the logging system
 *
 * @param level Log level (trace, debug, info, warn, error, critical)
 */
void init(spdlog::level::level_enum level = spdlog::level::info);

/**
 * @brief Get the default logger
 *
 * @return std::shared_ptr<spdlog::logger> The logger instance
 */
std::shared_ptr<spdlog::logger> get_logger();

} // namespace tekki::log

// Convenience macros
#define TEKKI_LOG_TRACE(...) ::tekki::log::get_logger()->trace(__VA_ARGS__)
#define TEKKI_LOG_DEBUG(...) ::tekki::log::get_logger()->debug(__VA_ARGS__)
#define TEKKI_LOG_INFO(...)  ::tekki::log::get_logger()->info(__VA_ARGS__)
#define TEKKI_LOG_WARN(...)  ::tekki::log::get_logger()->warn(__VA_ARGS__)
#define TEKKI_LOG_ERROR(...) ::tekki::log::get_logger()->error(__VA_ARGS__)
#define TEKKI_LOG_CRITICAL(...) ::tekki::log::get_logger()->critical(__VA_ARGS__)
