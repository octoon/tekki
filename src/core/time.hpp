// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#pragma once

#include <chrono>
#include <string>

namespace tekki::time {

using Clock = std::chrono::steady_clock;

/// 返回当前时间戳（秒，double 精度）
double now_seconds();

/// 将时间点格式化为可读字符串（HH:MM:SS.mmm）
std::string format_timestamp(const Clock::time_point& tp);

/// 用于统计帧间隔的计时器
class FrameTimer {
public:
    FrameTimer();

    /// 更新上一帧时间，返回本次间隔（秒）
    double tick();

    double delta_seconds() const { return m_delta_seconds; }
    double total_seconds() const;

private:
    Clock::time_point m_start;
    Clock::time_point m_last;
    double m_delta_seconds = 0.0;
};

/// 作用域计时器，在析构时输出日志
class ScopedTimer {
public:
    explicit ScopedTimer(std::string label);
    ~ScopedTimer();

private:
    std::string m_label;
    Clock::time_point m_begin;
};

} // namespace tekki::time
