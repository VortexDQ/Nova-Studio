#pragma once

#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>

namespace nova::core {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };

// Minimal, dependency-free, thread-safe logger.
// Real projects would swap this for spdlog; kept dependency-free here so the
// vertical slice builds with nothing beyond Qt + FFmpeg.
class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void log(LogLevel level, std::string_view module, std::string_view message) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::tm tmBuf{};
#if defined(_WIN32)
        localtime_s(&tmBuf, &time);
#else
        localtime_r(&time, &tmBuf);
#endif
        char timeStr[32];
        std::strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &tmBuf);

        std::ostream& out = (level >= LogLevel::Warn) ? std::cerr : std::cout;
        out << '[' << timeStr << "] [" << levelToString(level) << "] ["
            << module << "] " << message << '\n';
    }

    void setMinLevel(LogLevel level) { minLevel_ = level; }
    LogLevel minLevel() const { return minLevel_; }

private:
    Logger() = default;
    static constexpr const char* levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Debug: return "DEBUG";
            case LogLevel::Info:  return "INFO";
            case LogLevel::Warn:  return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Fatal: return "FATAL";
        }
        return "?";
    }

    std::mutex mutex_;
    LogLevel minLevel_ = LogLevel::Debug;
};

inline void logMessage(LogLevel level, std::string_view module, std::string_view message) {
    if (level >= Logger::instance().minLevel()) {
        Logger::instance().log(level, module, message);
    }
}

} // namespace nova::core

#define NOVA_LOG_INFO(module, msg)  ::nova::core::logMessage(::nova::core::LogLevel::Info,  module, msg)
#define NOVA_LOG_WARN(module, msg)  ::nova::core::logMessage(::nova::core::LogLevel::Warn,  module, msg)
#define NOVA_LOG_ERROR(module, msg) ::nova::core::logMessage(::nova::core::LogLevel::Error, module, msg)
#define NOVA_LOG_DEBUG(module, msg) ::nova::core::logMessage(::nova::core::LogLevel::Debug, module, msg)
