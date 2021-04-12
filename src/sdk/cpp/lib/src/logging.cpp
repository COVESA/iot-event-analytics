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

#include <ctime>
#include <iostream>
#include <mutex>
#include <string>

#include "util.hpp"

namespace iotea {
namespace core {
namespace log {

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
// Logger
//
Logger::Logger()
    : os_{std::cout.rdbuf()} {}

Logger* Logger::Get() {
    static Logger logger;
    return &logger;
}

Level Logger::GetLevel() const { return level_; }

void Logger::SetLevel(const Level level) { level_ = level; }

std::ostream& Logger::GetStream() { return os_; }

//
// LoggerFriend
//

LoggerFriend::LoggerFriend(Logger* logger, const Level level, int call_depth)
    : logger_{logger}
    , level_{level}
    , call_depth_{call_depth} {
    logger->mutex_.lock();
}

LoggerFriend::LoggerFriend(Logger* logger)
    : LoggerFriend{logger, Level::INFO, 0} {}

LoggerFriend::LoggerFriend(const LoggerFriend& other)
    : LoggerFriend{other.logger_, other.level_, other.call_depth_ + 1} {}

LoggerFriend::~LoggerFriend() {
    if (call_depth_ == 0) {
        *this << '\n';
    }
    logger_->mutex_.unlock();
}

LoggerFriend& LoggerFriend::operator<<(const std::ostream& (*f)(std::ostream&)) {
    if (logger_->GetLevel() <= level_) {
        logger_->GetStream() << f;
    }

    return *this;
}

//
// Friend functions
//
void SetLevel(const Level level) {
    LoggerFriend p{Logger::Get()};
    p.logger_->SetLevel(level);
}

LoggerFriend Log(const Level level) {
    LoggerFriend p{Logger::Get(), level};

    std::time_t now = ::time(nullptr);

    struct tm t;
    ::localtime_r(&now, &t);

    char tbuf[26];

    // %a: The abbreviated name of the day of the week according to the current locale.
    // %b: The abbreviated month name according to the current locale.
    // %d: The day of the month as a decimal number (range 01 to 31).
    // %H: The hour as a decimal number using a 24-hour clock (range 00 to 23).
    // %M: The minute as a decimal number (range 00 to 59).
    // %S: The second as a decimal number (range 00 to 60).
    // %Y; The year as a decimal number including the century.
    strftime(tbuf, sizeof(tbuf)/sizeof(tbuf[0]), "%a %b %d %H:%M:%S %Y", &t);

    static const char* tags[]{
        " [DEBUG] ",
        " [ INFO] ",
        " [ WARN] ",
        " [ERROR] ",
    };

    p << tbuf << tags[static_cast<int>(level)];

    return p;
}

LoggerFriend Debug() { return Log(Level::DEBUG); }

LoggerFriend Info() { return Log(Level::INFO); }

LoggerFriend Warn() { return Log(Level::WARNING); }

LoggerFriend Error() { return Log(Level::ERROR); }

}  // namespace log
}  // namespace core
}  // namespace iotea
