/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <regex>
#include <string>

#include "gtest/gtest.h"

#include "util.hpp"

using iotea::core::Uuid4;
using iotea::core::TopicExprMatcher;

/**
 * @brief Verify that Uuid4 generates properly formatted string representations
 * of UUID4s.
 */
TEST(util, Test_Uuid4) {
    Uuid4 id;
    std::string sid = id;
    std::string expr = R"([a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12})";

    ASSERT_TRUE(std::regex_match(sid, std::regex(expr)));
}

/**
 * @brief Verify that TopicExprMatcher::Match works properly
 */
TEST(util, TopicExprMatcher_Match) {
    struct {
        std::string topic_expr;
        std::string topic;
        bool expect_match;
    } tests[] {
        // Plain topic expressions
        {
            "iotea/plain/topic/nothing/special",
            "iotea/plain/topic/nothing/special",
            true,
        },
        {
            "iotea/plain/topic/nothing/special",
            "iotea/plain",
            false,
        },
        // Expressions ending with '#'
        {
            "iotea/topic/with/hash/#",
            "iotea/topic/with/hash",
            false,
        },
        {
            "iotea/topic/with/hash/#",
            "iotea/topic/with/hash/suffix1",
            true,
        },
        {
            "iotea/topic/with/hash/#",
            "iotea/topic/with/hash/suffix1/suffix2",
            true,
        },
        // Expressions containing '+'
        {
            "iotea/topic/+/with/plus",
            "iotea/topic/with/plus",
            false,
        },
        {
            "iotea/topic/+/with/plus",
            "iotea/topic/level/with/plus",
            true,
        },
        {
            "iotea/topic/+/+/with/plus",
            "iotea/topic/level1/level2/with/plus",
            true,
        },
        {
            "iotea/topic/with/plus/+",
            "iotea/topic/with/plus/level1",
            true,
        },
        {
            "iotea/topic/with/plus/+",
            "iotea/topic/with/plus/level1/level2",
            false,
        },
        // Expressions containing '.'
        {
            "iotea/topic/with.period",
            "iotea/topic/with.period",
            true,
        },
        {
            "iotea/topic.period/with.period",
            "iotea/topic.period/with.period",
            true,
        },
        // Expressions containing '$'
        {
            "iotea/topic/with/dollar/+$",
            "iotea/topic/with/dollar/level$",
            true,
        },
        {
            "iotea/topic/with$dollar",
            "iotea/topic/with$dollar",
            true,
        },
        {
            "iotea/topic$dollar/with$dollar",
            "iotea/topic$dollar/with$dollar",
            true,
        },
        {
            "iotea/topic/with/dollar$",
            "iotea/topic/with/dollar$",
            true,
        },
        // Expressions with both '#' and '+'
        {
            "iotea/topic/+/with/all/#",
            "iotea/topic/with/all",
            false,
        },
        {
            "iotea/topic/+/with/all/#",
            "iotea/topic/level1/with/all",
            false,
        },
        {
            "iotea/topic/+/with/all/#",
            "iotea/topic/level1/with/all/suffix1",
            true,
        },
        {
            "iotea/topic/with/all/+/+/#",
            "iotea/topic/with/all/level1/level2/suffix1",
            true,
        },
        {
            "iotea/topic/with/all/+/+/#",
            "iotea/topic/with/all/level1/level2/suffix1/suffix2",
            true,
        },
    };

    for (const auto& t : tests) {
        TopicExprMatcher matcher{t.topic_expr};
        ASSERT_EQ(matcher.Match(t.topic), t.expect_match);
    }
}
