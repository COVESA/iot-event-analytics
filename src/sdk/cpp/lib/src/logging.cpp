/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "logging.hpp"

#include <chrono>
#include <iostream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>

#include "util.hpp"

namespace iotea {
namespace core {
namespace logging {

Level GetLogLevel() {
    static const char* log_level_env = "IOTEA_LOG_LEVEL";
    static const char* tag_debug = "DEBUG";
    static const char* tag_info = "INFO";
    static const char* tag_warn = "WARN";
    static const char* tag_error = "ERROR";

    auto level = GetEnv(log_level_env, tag_info);

    if (level == tag_debug) {
        return Level::DEBUG;
    }

    if (level == tag_info) {
        return Level::INFO;
    }

    if (level == tag_warn) {
        return Level::WARNING;
    }

    if (level == tag_error) {
        return Level::ERROR;
    }

    return Level::INFO;
}

//
// InternalLogger
//
InternalLogger::InternalLogger()
    : os_{std::cout.rdbuf()} {}

InternalLogger* InternalLogger::Get() {
    static InternalLogger logger;
    return &logger;
}

Level InternalLogger::GetLevel() const { return level_; }

void InternalLogger::SetLevel(const Level level) { level_ = level; }

std::ostream& InternalLogger::GetStream() { return os_; }

//
// Logger
//
Logger::Logger(InternalLogger* logger, const Level level, int call_depth)
    : logger_{logger}
    , level_{level}
    , call_depth_{call_depth} {
    logger->mutex_.lock();
}

Logger::Logger(InternalLogger* logger)
    : Logger{logger, Level::INFO, 0} {}

Logger::Logger(const Logger& other)
    : Logger{other.logger_, other.level_, other.call_depth_ + 1} {}

Logger::~Logger() {
    if (call_depth_ == 0) {
        *this << '\n';
    }
    logger_->mutex_.unlock();
}

Logger& Logger::operator<<(const std::ostream& (*f)(std::ostream&)) {
    if (logger_->GetLevel() <= level_) {
        logger_->GetStream() << f;
    }

    return *this;
}

//
// Friend functions
//
void SetLevel(const Level level) {
    Logger p{InternalLogger::Get()};
    p.logger_->SetLevel(level);
}

Logger Log(const Level level) {
    auto now = std::chrono::system_clock::now();
    auto ts = std::chrono::system_clock::to_time_t(now);
    Logger p{InternalLogger::Get(), level};

    static const char* tags[]{
        " DEBUG ",
        "  INFO ",
        "  WARN ",
        " ERROR ",
    };

    std::ostringstream ss;
    ss << std::put_time(gmtime(&ts), "%FT%TZ");
    p <<  ss.str() << tags[static_cast<int>(level)];

    return p;
}

Logger Debug() { return Log(Level::DEBUG); }

Logger Info() { return Log(Level::INFO); }

Logger Warn() { return Log(Level::WARNING); }

Logger Error() { return Log(Level::ERROR); }

//
// NamedLogger
//
NamedLogger::NamedLogger(const std::string& name)
    : name_{name} {}

Logger NamedLogger::Debug() const {
    return logging::Debug() << name_ << " : ";
}

Logger NamedLogger::Info() const {
    return logging::Info() << name_ << " : ";
}

Logger NamedLogger::Warn() const {
    return logging::Warn() << name_ << " : ";
}

Logger NamedLogger::Error() const {
    return logging::Error() << name_ << " : ";

}

}  // namespace logging
}  // namespace core
}  // namespace iotea
