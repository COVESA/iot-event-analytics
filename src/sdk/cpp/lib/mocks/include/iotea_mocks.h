/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef MOCK_IOTEA_H_
#define MOCK_IOTEA_H_

#include <string>
#include <vector>

#include "gmock/gmock.h"

#include "iotea.hpp"

using iotea::core::reply_handler_ptr;
using iotea::core::publisher_ptr;
using iotea::core::uuid_generator_func_ptr;
using iotea::core::gather_func_ptr;
using iotea::core::gather_and_reply_func_ptr;
using iotea::core::timeout_func_ptr;
using iotea::core::Event;
using iotea::core::CallContext;
using iotea::core::CallToken;
using iotea::core::Callee;

namespace iotea {
namespace mock {
namespace core {

class Publisher : public iotea::core::Publisher {
   public:
    MOCK_METHOD(void, Publish, (const std::string& topic, const std::string& msg), (override));
};

class Receiver : public iotea::core::Receiver {
   public:
    MOCK_METHOD(void, Receive, (const std::string& topic, const std::string& msg), (override));
};

} // namespace core
} // namespace mock
} // namespace iotea

#endif // MOCK_IOTEA_H_

