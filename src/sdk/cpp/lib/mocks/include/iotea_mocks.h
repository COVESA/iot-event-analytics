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

using iotea::core::call_handler_ptr;
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

class CallHandler : public iotea::core::CallHandler {
   public:
    MOCK_METHOD(void, Gather, (gather_func_ptr func, timeout_func_ptr timeout_func, std::vector<CallToken> tokens));
    MOCK_METHOD(void, GatherAndReply, (gather_and_reply_func_ptr func, timeout_func_ptr timeout_func, const CallContext& ctx, std::vector<CallToken> tokens));
    MOCK_METHOD(void, HandleReply, (const std::string& token, const json& reply));
    MOCK_METHOD(void, HandleTick, (const int64_t& ts));
    MOCK_METHOD(int64_t, GetNowMs, (), (const));
};

class EventContext : public iotea::core::EventContext {
   public:
    EventContext() = default;
    MOCK_METHOD(std::string, GetTalentId, (), (const));
    MOCK_METHOD(std::string, GetSubject, (), (const));
    MOCK_METHOD(std::string, GetReturnTopic, (), (const));
    MOCK_METHOD(CallToken, Call, (const Callee& callee, const json& args, const int64_t& timeout), (const));
};

class CallContext : public iotea::core::CallContext {
   public:
    CallContext(const std::string& talent_id, const std::string& channel_id, const std::string& feature,
                const Event& event, call_handler_ptr call_handler, publisher_ptr publisher, uuid_generator_func_ptr uuid_gen)
        : iotea::core::CallContext{talent_id, channel_id, feature, event, call_handler, publisher, uuid_gen} {}

    MOCK_METHOD(void, Reply, (const json& value), (const override));
};

} // namespace core
} // namespace mock
} // namespace iotea

#endif // MOCK_IOTEA_H_

