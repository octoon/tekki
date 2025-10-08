// tekki - C++ port of kajiya renderer
// Copyright (c) 2025 tekki Contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "core/time.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

#include "core/log.h"

namespace tekki::time
{

using namespace std::chrono;

static std::string format_with_ms(const Clock::time_point& tp)
{
    // Convert steady_clock to system_clock for formatting
    const auto now_steady = Clock::now();
    const auto now_system = system_clock::now();
    const auto diff = duration_cast<system_clock::duration>(tp - now_steady);
    const auto system_tp = time_point_cast<system_clock::duration>(now_system + diff);

    const auto time = system_clock::to_time_t(system_tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%H:%M:%S");
    const auto remainder = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    oss << '.' << std::setfill('0') << std::setw(3) << remainder.count();
    return oss.str();
}

double now_seconds()
{
    const auto now = Clock::now();
    const auto since_epoch = duration_cast<duration<double>>(now.time_since_epoch());
    return since_epoch.count();
}

std::string format_timestamp(const Clock::time_point& tp)
{
    return format_with_ms(tp);
}

FrameTimer::FrameTimer() : m_start(Clock::now()), m_last(m_start) {}

double FrameTimer::tick()
{
    const auto now = Clock::now();
    m_delta_seconds = duration_cast<duration<double>>(now - m_last).count();
    m_last = now;
    return m_delta_seconds;
}

double FrameTimer::total_seconds() const
{
    const auto now = Clock::now();
    return duration_cast<duration<double>>(now - m_start).count();
}

ScopedTimer::ScopedTimer(std::string label) : m_label(std::move(label)), m_begin(Clock::now()) {}

ScopedTimer::~ScopedTimer()
{
    const auto end = Clock::now();
    const auto elapsed = duration_cast<duration<double, std::milli>>(end - m_begin).count();
    TEKKI_LOG_INFO("[timer] {} took {:.3f} ms", m_label, elapsed);
}

} // namespace tekki::time
