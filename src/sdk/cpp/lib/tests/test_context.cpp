/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "nlohmann/json.hpp"

#include "interface.hpp"
#include "context.hpp"

using json = nlohmann::json;

using namespace iotea::core;

/**
 * @brief Verify that EventContext::Emit sends properly formatted event
 * messages.
 */
TEST(context, EventContext_Emit) {
    class TestPublisher : public Publisher {
       public:
        MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    };

    auto publisher = std::make_shared<TestPublisher>();
    auto ctx = EventContext{"my_talent_id", "my_channel_id", "my_subject", "my_return_topic", nullptr, publisher, []{ return ""; }};

    // We can't expect anything for sure with respect to the published data
    // since it's JSON and we can't know a priori which of the different but
    // equivalent orders the object properties are going to be layed out in. So
    // we have to store the output in a string, parse it as JSON and compare it
    // "semantically" to what we expect.
    std::string raw_published_event;
    EXPECT_CALL(*publisher, Publish(::testing::_, ::testing::_)).Times(1).WillOnce(::testing::SaveArg<1>(&raw_published_event));

    ctx.Emit<std::string>("my_feature", "hello world", "my_type", "my_instance");
    auto published_event = json::parse(raw_published_event);

    // The timestamp in the outgoing event is set to the current epoch time in
    // ms, since we don't know what it will be we have to manually override it
    // before the final comparison.
    ASSERT_NO_THROW(published_event["whenMs"].get<int64_t>());
    published_event["whenMs"] = int64_t{1234};

    auto want = json::parse(R"({
        "subject": "my_subject",
        "feature": "my_feature",
        "value": "hello world",
        "type": "my_type",
        "instance": "my_instance",
        "whenMs": 1234
    })");

    ASSERT_EQ(published_event, want);
}

/**
 * @brief Verify that EventContext::Call sends properly formatted function call
 * messages.
 */
TEST(context, EventContext_Call) {
    class TestPublisher : public Publisher {
       public:
        MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    };

    auto publisher = std::make_shared<TestPublisher>();
    auto uuid_gen = []{ return "00000000-0000-0000-0000-000000000000"; };
    auto ctx = EventContext{"my_talent_id", "my_channel_id", "my_subject", "my_return_topic", nullptr, publisher, uuid_gen};
    auto callee = Callee{"target_talent_id", "target_func", "target_type"};

    // We can't expect anything for sure with respect to the published data
    // since it's JSON and we can't know a priori which of the different but
    // equivalent orders the object properties are going to be layed out in. So
    // we have to store the output in a string, parse it as JSON and compare it
    // "semantically" to what we expect.
    std::string raw_published_event;
    EXPECT_CALL(*publisher, Publish(::testing::_, ::testing::_)).Times(1).WillOnce(::testing::SaveArg<1>(&raw_published_event));

    // If the argument isn't an array we expect the SDK to wrap the argument in an array
    auto token = ctx.Call(callee, 42);
    ASSERT_EQ(token.GetCallId(), "00000000-0000-0000-0000-000000000000");

    auto published_event = json::parse(raw_published_event);

    // The timestamp in the outgoing event is set to the current epoch time in
    // ms, since we don't know what it will be we have to manually override it
    // before the final comparison. The same is true for the "timeoutAtMs"
    // property which is calculated based on the "whenMs" property.
    ASSERT_NO_THROW(published_event["whenMs"].get<int64_t>());
    published_event["whenMs"] = int64_t{1234};
    published_event["value"]["timeoutAtMs"] = int64_t{1234};

    auto want = json::parse(R"({
        "subject": "my_subject",
        "feature": "target_talent_id.target_func-in",
        "type": "target_type",
        "value": {
            "func": "target_func",
            "args": [42],
            "call": "00000000-0000-0000-0000-000000000000",
            "chnl": "my_channel_id",
            "timeoutAtMs": 1234
        },
        "whenMs": 1234
    })");

    ASSERT_EQ(published_event, want);
}

/**
 * @brief Test CallContext and verify that reply messages are correctly formatted.
 */
TEST(context, CallContext_Reply) {
    class PublisherMock : public Publisher {
       public:
        MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    };

    class TestCallContext : public CallContext {
       public:
        explicit TestCallContext(const Event& event, publisher_ptr publisher)
            : CallContext{"talent_id", "channel_id", "feature", event, std::make_shared<ReplyHandler>(), publisher, []{return "";}} {}
    };

    auto call_value = json{{"chnl", "caller_channel_id"}, {"call", "caller_call_id"}, {"timeoutAtMs", 0}};
    auto event = Event{"subject", "feature", call_value, "default", "default", "return_topic", 0};
    auto publisher = std::make_shared<PublisherMock>();

    auto ctx = TestCallContext{event, publisher};

    auto expect_published_reply = json::parse(R"({
        "feature":"talent_id.feature",
        "instance":"default",
        "subject":"subject",
        "type":"default",
        "value": {
            "$tsuffix": "/caller_channel_id/caller_call_id",
            "$vpath":"value",
            "value": { "key":"value"}
        },
        "whenMs":0
    })");

    // The timestamp set on the outgoing event can't be reached from here so we
    // have to store what's sent, unmarshal it, tweak the timestamp and then
    // verify that the rest of the event lives up to expectations.
    std::string raw_published_reply;
    EXPECT_CALL(*publisher, Publish(::testing::_, ::testing::_)).Times(1).WillOnce(::testing::SaveArg<1>(&raw_published_reply));
    ctx.Reply(json{{"key", "value"}});

    auto published_reply = json::parse(raw_published_reply);
    published_reply["whenMs"] = 0;
    ASSERT_EQ(published_reply, expect_published_reply);
}

