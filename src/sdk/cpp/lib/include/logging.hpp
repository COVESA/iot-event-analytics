/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_LOGGING_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_LOGGING_HPP_

#include <iostream>
#include <mutex>

namespace iotea {
namespace core {
namespace log {

enum class Level { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3 };

Level GetLogLevel();

class LoggerFriend;

class Logger {
   private:
    std::recursive_mutex mutex_;
    std::ostream os_;
    Level level_ = GetLogLevel();

    Logger();

    static Logger* Get();

    Level GetLevel() const;

    void SetLevel(const Level level);

    std::ostream& GetStream();

    friend class LoggerFriend;
    friend void SetLevel(const Level);
    friend LoggerFriend Log(const Level);
};

class LoggerFriend {
   private:
    Logger* logger_;
    const Level level_ = Level::INFO;
    const int call_depth_ = -1;

    LoggerFriend(Logger* l, const Level level, int call_depth = 0);

    explicit LoggerFriend(Logger* l);

    LoggerFriend(const LoggerFriend& other);

    LoggerFriend& operator=(const LoggerFriend& other) = delete;

   public:
    ~LoggerFriend();

    LoggerFriend& operator<<(const std::ostream& (*f)(std::ostream&));

    template <typename T>
    LoggerFriend& operator<<(const T& t) {
        if (logger_->GetLevel() <= level_) {
            logger_->GetStream() << t;
        }

        return *this;
    }

    friend void SetLevel(const Level);
    friend LoggerFriend Log(const Level);
    friend LoggerFriend Debug();
    friend LoggerFriend Info();
    friend LoggerFriend Warn();
    friend LoggerFriend Error();
};

void SetLevel(const Level lvl);

LoggerFriend Log(const Level lvl);

LoggerFriend Debug();
LoggerFriend Info();
LoggerFriend Warn();
LoggerFriend Error();

}  // namespace log
}  // namespace core
}  // namespace iotea

#endif  // SRC_SDK_CPP_LIB_INCLUDE_LOGGING_HPP_
