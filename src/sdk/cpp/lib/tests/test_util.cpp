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

/**
 * @brief Verify that Uuid4 generates properly formatted string representations
 * of UUID4s.
 */
TEST(iotea, Test_Uuid4) {
    Uuid4 id;
    std::string sid = id;
    std::string expr = R"([a-f0-9]{8}(?:-[a-f0-9]{4}){3}-[a-f0-9]{12})";

    ASSERT_TRUE(std::regex_match(sid, std::regex(expr)));
}
