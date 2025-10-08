// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "core/log.h"

#include <spdlog/sinks/stdout_color_sinks.h>

namespace tekki::log
{

static std::shared_ptr<spdlog::logger> s_logger;

void init(spdlog::level::level_enum level)
{
    s_logger = spdlog::stdout_color_mt("tekki");
    s_logger->set_level(level);
    s_logger->set_pattern("[%T] [%^%l%$] %v");

    s_logger->info("tekki logging system initialized");
}

std::shared_ptr<spdlog::logger> get_logger()
{
    if (!s_logger)
    {
        init();
    }
    return s_logger;
}

} // namespace tekki::log
