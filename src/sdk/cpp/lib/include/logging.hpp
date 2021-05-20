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
namespace logging {

enum class Level { DEBUG = 0, INFO = 1, WARNING = 2, ERROR = 3 };

Level GetLogLevel();

class Logger;

class InternalLogger {
   private:
    std::recursive_mutex mutex_;
    std::ostream os_;
    Level level_ = GetLogLevel();

    InternalLogger();

    static InternalLogger* Get();

    Level GetLevel() const;

    void SetLevel(const Level level);

    std::ostream& GetStream();

    friend class Logger;
    friend void SetLevel(const Level);
    friend Logger Log(const Level);
};

class Logger {
   public:

    Logger(const Logger& other);

    Logger& operator=(const Logger& other) = default;

    ~Logger();

    Logger& operator<<(const std::ostream& (*f)(std::ostream&));

    template <typename T>
    Logger& operator<<(const T& t) {
        if (logger_->GetLevel() <= level_) {
            logger_->GetStream() << t;
        }

        return *this;
    }

    friend class NamedLogger;
    friend void SetLevel(const Level);
    friend Logger Log(const Level);
    friend Logger Debug();
    friend Logger Info();
    friend Logger Warn();
    friend Logger Error();

   private:
    InternalLogger* logger_;
    Level level_ = Level::INFO;
    int call_depth_ = -1;

    Logger(InternalLogger* l, const Level level, int call_depth = 0);

    explicit Logger(InternalLogger* l);

};

void SetLevel(const Level lvl);

Logger Log(const Level lvl);

Logger Debug();
Logger Info();
Logger Warn();
Logger Error();

class NamedLogger {
   public:
    explicit NamedLogger(const std::string& name);

    Logger Debug() const;
    Logger Info() const;
    Logger Warn() const;
    Logger Error() const;

   private:
    std::string name_;
};


}  // namespace logging
}  // namespace core
}  // namespace iotea

#endif  // SRC_SDK_CPP_LIB_INCLUDE_LOGGING_HPP_
