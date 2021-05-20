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

#include "nlohmann/json.hpp"

#include "event.hpp"

using json = nlohmann::json;

using namespace iotea::core;


/**
 * @brief Test Message unmarshalling and accessors methods.
 */
TEST(event, Message) {
    struct {
        bool is_event;
        bool is_discover;
        bool is_error;
        int code;

        json raw;
    } tests[] {
        {
            true,
            false,
            false,
            0,

            json::parse(R"({"msgType": 1})")
        },
        {
            false,
            true,
            false,
            0,

            json::parse(R"({"msgType": 2})")
        },
        {
            false,
            false,
            true,
            1234,

            json::parse(R"({"msgType": 4, "code": 1234})")
        },
    };

    for (const auto& t : tests) {
        const auto m = Message::FromJson(t.raw);

        ASSERT_EQ(t.is_event, m.IsEvent());
        ASSERT_EQ(t.is_discover, m.IsDiscover());
        ASSERT_EQ(t.is_error, m.IsError());
        ASSERT_EQ(t.code, m.GetCode());
    }
}

/**
 * @brief Test DiscoverMessage unmarshalling and accessors methods.
 */
TEST(event, DiscoverMessage) {
    struct {
        std::string version;
        std::string return_topic;

        json raw;
    } tests[] {
        {
            "0.0.0",
            "alpha/beta/gamma",

            json::parse(R"({"msgType": 2, "returnTopic": "alpha/beta/gamma"})")
        },
        {
            "1.0.0",
            "alpha/beta/gamma",

            json::parse(R"({"msgType": 2, "version": "1.0.0", "returnTopic": "alpha/beta/gamma"})")
        },
    };

    for (const auto& t : tests) {
        const auto m = DiscoverMessage::FromJson(t.raw);

        ASSERT_EQ(t.version, m.GetVersion());
        ASSERT_EQ(t.return_topic, m.GetReturnTopic());
    }
}

/**
 * @brief Test ErrorMessage unmarshalling and accessors methods.
 */
TEST(event, ErrorMessage) {
    struct {
        int code;
        std::string message;

        json raw;
    } tests[] {
        {
            4000,
            "non prefixed output feature found",

            json::parse(R"({"msgType": 4, "code": 4000})")
        },
        {
            4001,
            "feature dependency loop found",

            json::parse(R"({"msgType": 4, "code": 4001})")
        },
        {
            4002,
            "invalid discovery info",

            json::parse(R"({"msgType": 4, "code": 4002})")
        },
        {
            4003,
            "error resolving given segment in the talent ruleset",

            json::parse(R"({"msgType": 4, "code": 4003})")
        },
        {
            -1,
            "unknown error",

            json::parse(R"({"msgType": 4, "code": -1})")
        },
    };

    for (const auto& t : tests) {
        const auto m = ErrorMessage::FromJson(t.raw);

        ASSERT_EQ(t.code, m.GetCode());
        ASSERT_EQ(t.message, m.GetMessage());
    }
}

/**
 * @brief Test Event unmarshalling and accessors methods.
 */

TEST(event, PlatformEvent) {
    struct {
        PlatformEvent::Type type;
        json data;
        int64_t timestamp;

        json raw;
    } tests[] {
        {
            PlatformEvent::Type::TALENT_RULES_SET,
            json::object(),
            12345,
            json::parse(R"({
                "type": "platform.talent.config.set",
                "data": {},
                "timestamp": 12345
            })")
        },
        {
            PlatformEvent::Type::TALENT_RULES_UNSET,
            json::object(),
            12345,
            json::parse(R"({
                "type": "platform.talent.config.unset",
                "data": {},
                "timestamp": 12345
            })")
        }
    };

    for (const auto& t : tests) {
        auto have = PlatformEvent::FromJson(t.raw);

        ASSERT_EQ(have.GetType(), t.type);
        ASSERT_EQ(have.GetData(), t.data);
        ASSERT_EQ(have.GetTimestamp(), t.timestamp);
    }
}

/**
 * @brief Test Event unmarshalling and accessors methods.
 */
TEST(event, Event) {
    auto subject = "test_subject";
    auto feature = "test_feature";
    auto value = json{{"test", "json"}};
    auto type = "test_type";
    auto instance = "test_instance";
    auto return_topic = "test/return/topic";
    auto when = int64_t{1615209290000};

    auto event = Event(subject, feature, value, type, instance, return_topic, when);

    // Test that the Json/FromJson methods are inverses operations
    auto alpha = event.Json();
    auto beta = Event::FromJson(alpha);
    auto gamma = beta.Json();
    ASSERT_EQ(alpha, gamma);

    ASSERT_EQ(beta.GetSubject(), subject);
    ASSERT_EQ(beta.GetFeature(), feature);
    ASSERT_EQ(beta.GetValue(), value);
    ASSERT_EQ(beta.GetType(), type);
    ASSERT_EQ(beta.GetInstance(), instance);
    // The return topic is dropped during marshalling because its
    // serialization occurs when events are sent as replies.
    ASSERT_EQ(beta.GetWhen(), when);
}

/**
 * @brief Verify that Event::FromJson can build an Event object from JSON.
 */
TEST(event, Event_FromJson) {
    auto payload = json::parse(R"({
            "cid": "2294a18a-4179-491f-828c-7b615602f86f",
            "feature": "temp",
            "instance": "1",
            "msgType": 1,
            "now": 1618550741573,
            "returnTopic": "123456/ingestion/events",
            "segment": "100000",
            "subject": "booliboo",
            "type": "kuehlschrank",
            "value": 5,
            "whenMs": 1618550741000
        })");

    auto event = Event::FromJson(payload);

    ASSERT_EQ(event.GetFeature(), "temp");
    ASSERT_EQ(event.GetInstance(), "1");
    ASSERT_EQ(event.GetReturnTopic(), "123456/ingestion/events");
}
