/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_UTIL_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_UTIL_HPP_

#include <regex>
#include <string>

namespace iotea {
namespace core {

/**
 * @brief Uuid4 generates UUID4s
 */
class Uuid4 {
   public:
    /**
     * @brief Constructs a new Uuid4 object.
     */
    Uuid4() noexcept;

    operator std::string() const noexcept;

   private:
    void Generate();
    void Stringify();

    uint8_t bits_[16];
    std::string str_;
};

class TopicExprMatcher {
   public:
    TopicExprMatcher(std::string topic_expr);

    bool Match(const std::string& topic) const;

   private:
    void ReplaceAll(std::string& s, const std::string& what, const std::string& with);

    std::regex expr_;
};

std::string GetEnv(const std::string& name, const std::string& defval = "");
int64_t GetEpochTimeMs();

std::string GenerateUUID();

}  // namespace core
}  // namespace iotea

#endif  // SRC_SDK_CPP_LIB_INCLUDE_UTIL_HPP_
