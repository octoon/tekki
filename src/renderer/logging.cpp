#include "tekki/renderer/logging.h"
#include <memory>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <functional>
#include <stdexcept>

namespace tekki::renderer {

Logger::ColoredLevelConfig::ColoredLevelConfig() = default;

Logger::ColoredLevelConfig& Logger::ColoredLevelConfig::Error(const std::string& color) {
    errorColor = color;
    return *this;
}

Logger::ColoredLevelConfig& Logger::ColoredLevelConfig::Warn(const std::string& color) {
    warnColor = color;
    return *this;
}

Logger::ColoredLevelConfig& Logger::ColoredLevelConfig::Info(const std::string& color) {
    infoColor = color;
    return *this;
}

Logger::ColoredLevelConfig& Logger::ColoredLevelConfig::Debug(const std::string& color) {
    debugColor = color;
    return *this;
}

Logger::ColoredLevelConfig& Logger::ColoredLevelConfig::Trace(const std::string& color) {
    traceColor = color;
    return *this;
}

std::string Logger::ColoredLevelConfig::GetColor(LogLevel level) const {
    switch (level) {
        case LogLevel::Error: return errorColor;
        case LogLevel::Warn: return warnColor;
        case LogLevel::Info: return infoColor;
        case LogLevel::Debug: return debugColor;
        case LogLevel::Trace: return traceColor;
        default: return "\x1B[37m";
    }
}

std::string Logger::ColoredLevelConfig::Color(LogLevel level) const {
    return GetColor(level);
}

Logger::LogDispatch::LogDispatch() = default;

Logger::LogDispatch& Logger::LogDispatch::Format(std::function<std::string(LogLevel, const std::string&, const std::string&)> formatter) {
    return *this;
}

Logger::LogDispatch& Logger::LogDispatch::Level(LogLevel level) {
    defaultLevel = level;
    return *this;
}

Logger::LogDispatch& Logger::LogDispatch::LevelFor(const std::string& target, LogLevel level) {
    levelOverrides.emplace_back(target, level);
    return *this;
}

Logger::LogDispatch& Logger::LogDispatch::Chain(std::ostream& stream) {
    outputFunc = [&stream](const std::string& message) {
        stream << message << std::endl;
    };
    return *this;
}

Logger::LogDispatch& Logger::LogDispatch::Chain(std::shared_ptr<std::ofstream> file) {
    outputFunc = [file](const std::string& message) {
        if (file && file->is_open()) {
            *file << message << std::endl;
        }
    };
    return *this;
}

void Logger::LogDispatch::Apply() {
}

void Logger::SetUpLogging(LogLevel defaultLogLevel) {
    // configure colors for the whole line
    ColoredLevelConfig colorsLine;
    colorsLine.Error("\x1B[31m")
              .Warn("\x1B[33m")
              .Info("\x1B[37m")
              .Debug("\x1B[37m")
              .Trace("\x1B[90m");

    // configure colors for the name of the level.
    // since almost all of them are the same as the color for the whole line, we
    // just clone `colorsLine` and overwrite our changes
    ColoredLevelConfig colorsLevel = colorsLine;
    colorsLevel.Info("\x1B[32m");

    // here we set up our fern Dispatch
    auto consoleOut = LogDispatch();
    consoleOut.Format([colorsLine, colorsLevel](LogLevel level, const std::string& target, const std::string& message) -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        
        std::string levelStr;
        switch (level) {
            case LogLevel::Error: levelStr = "ERROR"; break;
            case LogLevel::Warn: levelStr = "WARN"; break;
            case LogLevel::Info: levelStr = "INFO"; break;
            case LogLevel::Debug: levelStr = "DEBUG"; break;
            case LogLevel::Trace: levelStr = "TRACE"; break;
        }

        return colorsLine.GetColor(level) + 
               "[" + ss.str() + "][" + target + "][" + colorsLevel.Color(level) + levelStr + colorsLine.GetColor(level) + "] " + 
               message + "\x1B[0m";
    })
    // set the default log level. to filter out verbose log messages from dependencies, set
    // this to Warn and overwrite the log level for your crate.
    .Level(defaultLogLevel)
    // change log levels for individual modules. Note: This looks for the record's target
    // field which defaults to the module path but can be overwritten with the `target`
    // parameter:
    // `info!(target="special_target", "This log message is about special_target");`
    // .LevelFor("kajiya::device", LogLevel::Trace)
    // output to stdout
    .Chain(std::cout);

    auto file = std::make_shared<std::ofstream>();
    file->open("output.log", std::ios::out | std::ios::trunc);

    if (!file->is_open()) {
        throw std::runtime_error("Failed to open log file");
    }

    auto fileOut = LogDispatch();
    fileOut.Format([](LogLevel level, const std::string& target, const std::string& message) -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        
        std::string levelStr;
        switch (level) {
            case LogLevel::Error: levelStr = "ERROR"; break;
            case LogLevel::Warn: levelStr = "WARN"; break;
            case LogLevel::Info: levelStr = "INFO"; break;
            case LogLevel::Debug: levelStr = "DEBUG"; break;
            case LogLevel::Trace: levelStr = "TRACE"; break;
        }

        return "[" + ss.str() + "][" + target + "][" + levelStr + "] " + message;
    })
    .Level(LogLevel::Trace)
    .LevelFor("async_io", LogLevel::Warn)
    .LevelFor("polling", LogLevel::Warn)
    .Chain(file);

    // Combine and apply both dispatches
    try {
        consoleOut.Apply();
        fileOut.Apply();
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to set up logging: ") + e.what());
    }
}

} // namespace tekki::renderer