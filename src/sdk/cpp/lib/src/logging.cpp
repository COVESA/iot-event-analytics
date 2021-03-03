/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/
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
        operator<<('\n');
    }
    logger_->mutex_.unlock();
}

LoggerFriend& LoggerFriend::operator<<(std::ostream& (*f)(std::ostream&)) {
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

    // From man (3) asctime_r
    // asctime_r .. stores the string in a user-supplied buffer which should
    // have room for at least 26 bytes
    char tbuf[26];
    ::asctime_r(&t, tbuf);

    // The string returned by asctime_r ends with a \n
    std::string timestamp{tbuf};
    timestamp = timestamp.substr(0, timestamp.length() - 1);

    static const char* tags[]{
        " [DEBUG] ",
        " [ INFO] ",
        " [ WARN] ",
        " [ERROR] ",
    };

    p << timestamp << tags[static_cast<int>(level)];

    return p;
}

LoggerFriend Debug() { return Log(Level::DEBUG); }

LoggerFriend Info() { return Log(Level::INFO); }

LoggerFriend Warn() { return Log(Level::WARNING); }

LoggerFriend Error() { return Log(Level::ERROR); }

}  // namespace log
}  // namespace core
}  // namespace iotea
