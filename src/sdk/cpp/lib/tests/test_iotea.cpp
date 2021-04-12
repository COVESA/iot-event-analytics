/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <chrono>
#include <memory>
#include <vector>
#include <utility>
#include <tuple>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include "iotea.hpp"
#include "schema.hpp"
#include "iotea_mocks.h"

using json = nlohmann::json;

using namespace iotea::core;

/**
 * @brief Test that a Talent retains its assigned name.
 */
TEST(iotea, Talent_GetId) {
  auto talent = Talent("test_talent");
  ASSERT_STREQ(talent.GetId().c_str(), "test_talent");
}

/**
 * @brief Test different rule set permutations and verify that the Talent
 * generates the correspnding JSON schema correctly.
 */
TEST(iotea, Talent_OnGetRules) {
    class GetRulesTalent : public Talent {
       public:
        schema::rules_ptr rules_;

        explicit GetRulesTalent(schema::rules_ptr rules)
            : Talent{"test_OnGetRules"}
            , rules_{rules} {}

        schema::rule_ptr OnGetRules() const override {
            return rules_;
        }
    };

    struct {
        schema::rules_ptr rules;
        json want;
    } tests[] {
        {
            nullptr,
            nullptr,
        },
        {
            OrRules(IsSet("alpha", "thing")),
            json::parse(R"({
                                "rules": [
                                    {
                                        "feature": "alpha",
                                        "instanceIdFilter": ".*",
                                        "limitFeatureSelection": true,
                                        "op": 0,
                                        "path": "",
                                        "typeSelector": "thing",
                                        "value": {
                                            "not": {
                                                "type": "null"
                                            }
                                        },
                                        "valueType": 0
                                    }
                                ],
                                "type": "or",
                                "excludeOn": null
                            })"),
        },
        {
            OrRules(IsSet("alpha", "thing"), IsSet("beta", "thang")),
            json::parse(R"({
                                "rules": [
                                    {
                                        "feature": "alpha",
                                        "instanceIdFilter": ".*",
                                        "limitFeatureSelection": true,
                                        "op": 0,
                                        "path": "",
                                        "typeSelector": "thing",
                                        "value": {
                                            "not": {
                                                "type": "null"
                                            }
                                        },
                                        "valueType": 0
                                    },
                                    {
                                        "feature": "beta",
                                        "instanceIdFilter": ".*",
                                        "limitFeatureSelection": true,
                                        "op": 0,
                                        "path": "",
                                        "typeSelector": "thang",
                                        "value": {
                                            "not": {
                                                "type": "null"
                                            }
                                        },
                                        "valueType": 0
                                    }
                                ],
                                "type": "or",
                                "excludeOn": null
                            })"),
        },
    };

    for (const auto &t : tests) {
        auto talent = GetRulesTalent(t.rules);

        auto have = talent.OnGetRules();

        ASSERT_EQ(have ? have->Json() : json(nullptr), t.want);
    }
}

/**
 * @brief Test creating different Callees and verify that the Talent generates
 * the correspnding JSON schema correctly.
 */
TEST(iotea, Talent_RegisterCallee) {
    struct {
        std::vector<Callee> callees;
        uuid_generator_func_ptr uuid_gen;
        json want;
    } tests[] {
        {
            {{ "talent_name", "function_name", "" }},
            []{ return "00000000-0000-0000-0000-000000000000"; },
            json::parse(R"({
                    "rules": [
                        {
                            "feature": "talent_name.function_name-out",
                            "instanceIdFilter": ".*",
                            "limitFeatureSelection": true,
                            "op": 0,
                            "path": "/$tsuffix",
                            "typeSelector": "default",
                            "value": {
                                "pattern": "^\\/test_RegisterCallee\\.[^\\/]+\\/.*",
                                "type": "string"
                            },
                            "valueType": 0
                        }
                    ],
                    "type": "or",
                    "excludeOn": null
                })")
        },
        {
            {
                { "talent_name1", "function_name1", "" },
                { "talent_name2", "function_name2", ""}
            },
            []{ return "00000000-0000-0000-0000-000000000000"; },
            json::parse(R"({
                    "rules": [
                        {
                            "feature": "talent_name1.function_name1-out",
                            "instanceIdFilter": ".*",
                            "limitFeatureSelection": true,
                            "op": 0,
                            "path": "/$tsuffix",
                            "typeSelector": "default",
                            "value": {
                                "pattern": "^\\/test_RegisterCallee\\.[^\\/]+\\/.*",
                                "type": "string"
                            },
                            "valueType": 0
                        },
                        {
                            "feature": "talent_name2.function_name2-out",
                            "instanceIdFilter": ".*",
                            "limitFeatureSelection": true,
                            "op": 0,
                            "path": "/$tsuffix",
                            "typeSelector": "default",
                            "value": {
                                "pattern": "^\\/test_RegisterCallee\\.[^\\/]+\\/.*",
                                "type": "string"
                            },
                            "valueType": 0
                        }
                    ],
                    "type": "or",
                    "excludeOn": null
                })")
        },
    };

    for (const auto&t : tests) {
        auto talent = Talent("test_RegisterCallee");

        talent.Initialize(std::make_shared<CallHandler>(), nullptr, t.uuid_gen);

        for (const auto& c : t.callees) {
            talent.RegisterCallee(c.GetTalentId(), c.GetFunc(), c.GetType());
        }

        auto have = talent.GetRules();

        ASSERT_EQ(have->Json(), t.want);
    }
}

/**
 * @brief Test that a Talent can call a function and gather the results.
 */
TEST(itoea, Talent_CallAndGather) {
    // This client creates a callee. When it receives an event, it extracts the
    // payload from the event and calls the callee with the payload and assigns
    // the reply from the callee to the member variable reply_
    class TestTalent : public Talent {
       public:
        Callee callee_;
        json reply_;

        explicit TestTalent(const std::string& talent_id)
            : Talent{talent_id} {
                callee_ = RegisterCallee("some_talent", "some_func");
            }

        void OnEvent(const Event& event, EventContext ctx) override {
            auto token = ctx.Call(callee_, event.GetValue());

            auto reply_handler = [this](const json& reply) {
                reply_ = reply;
            };
            ctx.Gather(reply_handler, nullptr, token);
        }
    };

    // Create and set up the talent
    std::string talent_id{"test_Talent"};
    auto talent = TestTalent{talent_id};
    auto call_handler = std::make_shared<CallHandler>();

    std::string fake_uuid{"00000000-0000-0000-0000-000000000000"};
    auto uuid_gen = [fake_uuid]{ return fake_uuid; };
    talent.Initialize(call_handler, nullptr, uuid_gen);

    // Create what's needed to send an event to the Talent
    auto publisher = std::make_shared<testing::NiceMock<iotea::mock::core::Publisher>>();
    auto ctx = EventContext{"talent_id", "channel_id", "subject",
        "ingestion/events", call_handler, publisher, uuid_gen};

    auto value = json{"hello", "world"};
    auto event = Event{"subject", "feature", value};

    // The following sequence of events follow
    // 1. Post the event to the talent
    // 2. Talent calls function and gathers (waits for) the reply
    // 3. Generate a reply send it to the CallHandler
    // 4. The CallHandler gathers the reply and sends it to the talent which
    //    assigns the reply the member "reply_"
    // 5. The talent's reply_ member is compared to the sent reply

    // 1 & 2
    talent.OnEvent(event, ctx);

    // 3 & 4
    call_handler->HandleReply(fake_uuid, value);

    // The replies passed to the talent's handler function are packed into a
    // JSON array in the order of the corresponding call tokens.
    auto expect = json::array({value}); // Expect one reply, array with one element
    ASSERT_EQ(talent.reply_, expect);
}

/**
 * @brief Test that Gatherer handles timeouts.
 */
TEST(iotea, Gatherer_CheckTimeout) {
    class TestGatherer : public Gatherer {
       public:
        TestGatherer(timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms)
            : Gatherer(timeout_func, tokens, now_ms) {}

        void Call() const override {}
    };

    struct {
        std::vector<CallToken> tokens;
        // The current time and the expected result
        std::vector<std::pair<int64_t, bool>> in_out_pairs;
    } tests[] {
        {
            // Single token without timeout, should not timeout
            {CallToken{"a", -1}},
            {{0, false}, {100, false}, {200, false}},
        },
        {
            // Single token, should timeout at t=100
            {CallToken{"a", 100}},
            {{0, false}, {100, true}, {200, true}},
        },
        {
            // Multiple tokens, should timeout at earliest timeout, i.e. t=100
            {CallToken{"a", 100}, CallToken{"b", 200}},
            {{0, false}, {100, true}, {200, true}},
        },
        {
            // Multiple tokens of which one doesn't have a timeout, should timeout at earliest timeout, i.e. t=100
            {CallToken{"a", 100}, CallToken{"b", 200}, CallToken{"c", -1}},
            {{0, false}, {100, true}, {200, true}},
        }
    };

    // Test input without timeout callback function
    for (const auto& t : tests) {
        auto gatherer = TestGatherer{nullptr, t.tokens, 0};

        for (const auto& iop : t.in_out_pairs) {
            auto have = gatherer.CheckTimeout(iop.first);
            ASSERT_EQ(have, iop.second);
        }
    }

    // Test input with timeout function
    for (const auto& t : tests) {
        auto timed_out = false;
        auto f = [&timed_out]{ timed_out = true; };
        auto gatherer = TestGatherer{f, t.tokens, 0};

        for (const auto& iop : t.in_out_pairs) {
            auto have = gatherer.CheckTimeout(iop.first);
            ASSERT_EQ(have, iop.second);
            ASSERT_EQ(have, timed_out);
        }
    }
}

/**
 * @brief Test that Gatherer correctly distinguishes between call IDs (tokens)
 * that it does and doesn't depend on.
 */
TEST(iotea, Gatherer_Wants) {
    class TestGatherer : public Gatherer {
       public:
        TestGatherer(timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens)
            : Gatherer(timeout_func, tokens) {}

        void Call() const override {}
    };

    struct {
        std::vector<CallToken> tokens;
        std::vector<std::pair<std::string, bool>> in_out_pairs;
    } tests[] {
        {
            {CallToken{"b"}},
            {{"a", false}, {"b", true}, {"c", false}}
        },
        {
            {CallToken{"a"}, CallToken{"b"}, CallToken{"c"}},
            {{"a", true}, {"b", true}, {"c", true}}
        },
        {
            {CallToken{"a"}, CallToken{"b"}, CallToken{"c"}},
            {{"d", false}, {"b", true}, {"e", false}}
        }
    };

    for (const auto& t : tests) {
        auto gatherer = TestGatherer{nullptr, t.tokens};

        for (const auto& iop : t.in_out_pairs) {
            auto have = gatherer.Wants(iop.first);
            ASSERT_EQ(have, iop.second);
        }
    }
}

/**
 * @brief Test that Gatherer only collects replies corresponding to the call
 * IDs it depends on. Verify that replies are stored correctly and that the
 * "Call" callback is invoked when all the call IDs have been gathered.
 */
TEST(iotea, Gatherer_Gather) {
    class TestGatherer : public Gatherer {
       public:
        bool& called_;
        TestGatherer(const std::vector<CallToken>& tokens, bool& called)
            : Gatherer(nullptr, tokens)
            , called_{called} {}

        std::unordered_map<std::string, json> GetReplies() const {
            return replies_;
        }

        void Call() const override {
            called_ = true;
        }
    };

    auto tokens = std::vector<CallToken>{CallToken{"a"}, CallToken{"b"}, CallToken{"c"}};
    auto called = false;
    auto gatherer = TestGatherer{tokens, called};

    auto in_out_tuple = std::vector<std::tuple<std::string, json, bool>>{
        {"d", json{{"value", "d"}}, false},
        {"a", json{{"value", "a"}}, false},
        {"e", json{{"value", "e"}}, false},
        {"b", json{{"value", "b"}}, false},
        {"f", json{{"value", "f"}}, false},
        {"c", json{{"value", "c"}}, true},
    };

    for (const auto& tup : in_out_tuple) {
        std::string token;
        json reply;
        bool expect;

        std::tie(token, reply, expect) = tup;
        auto have = gatherer.Gather(token, reply);
        ASSERT_EQ(have, expect);
        ASSERT_EQ(have, called);

        if (gatherer.Wants(token)) {
            ASSERT_EQ(gatherer.GetReplies()[token], reply);
        }
    }
}

/**
 * @brief Test that SinkGatherer only collects replies corresponding to the
 * call IDs it depends on. Verify that replies are stored correctly and that
 * the supplied callback function is invoked when all the call IDs have been
 * gathered.
 */
TEST(iotea, SinkGatherer_Gather) {
    auto tokens = std::vector<CallToken>{CallToken{"a"}, CallToken{"b"}, CallToken{"c"}};

    auto value_a = json{{"value", "a"}};
    auto value_b = json{{"value", "b"}};
    auto value_c = json{{"value", "c"}};
    auto in_out_tuple = std::vector<std::tuple<std::string, json, bool>>{
        {"d", json{{"value", "d"}}, false},
        {"a", value_a, false},
        {"e", json{{"value", "e"}}, false},
        {"b", value_b, false},
        {"f", json{{"value", "f"}}, false},
        {"c", value_c, true},
    };

    std::vector<json> gathered_replies;
    auto gather_func = [&gathered_replies](std::vector<json> replies) { gathered_replies = replies; };
    auto gatherer = SinkGatherer{gather_func, nullptr, tokens};

    for (const auto& tup : in_out_tuple) {
        std::string token;
        json reply;
        bool expect;

        std::tie(token, reply, expect) = tup;
        auto finished = gatherer.Gather(token, reply);
        ASSERT_EQ(finished, expect);

        if (finished) {
            ASSERT_EQ(gathered_replies, json({value_a, value_b, value_c}));
        }
    }
}

/**
 * @brief Test that ReplyGatherer only collects replies corresponding to the
 * call IDs it depends on. Verify that replies are stored correctly and that
 * the supplied callback function is invoked and returns as expected when all
 * the call IDs have been gathered.
 */
/* TODO re-think mocking and re-implement
TEST(iotea, ReplyGatherer_Gather) {
    std::string subject{"subject"};
    std::string feature{"feature"};
    std::string channel_id{"00000000-0000-0000-0000-000000000000"};
    std::string call_id{"00000000-0000-0000-0000-000000000000"};
    std::string type{"default"};
    std::string instance{"default"};
    std::string return_topic{"return_topic"};
    auto call_value = json{{"chnl", channel_id}, {"call", call_id}, {"args", json{}}};
    auto event = Event{subject, feature, call_value, type, instance, return_topic};

    std::string talent_id{"talent_id"};
    auto gather_func = [](std::vector<json> replies) { return replies; };
    auto call_handler = std::make_shared<CallHandler>();
    auto publisher = std::make_shared<::testing::NiceMock<iotea::mock::core::Publisher>>();
    std::string fake_uuid{"00000000-0000-0000-0000-000000000000"};
    auto uuid_gen = [fake_uuid]{ return fake_uuid; };
    //auto ctx = CallContext{talent_id, channel_id, feature, event, call_handler, publisher, uuid_gen};
    iotea::mock::core::CallContext ctx{talent_id, channel_id, feature, event, call_handler, publisher, uuid_gen};

    auto value_a = json{{"value", "a"}};
    auto value_b = json{{"value", "b"}};
    auto value_c = json{{"value", "c"}};
    auto in_out_tuple = std::vector<std::tuple<std::string, json, bool>>{
        {"d", json{{"value", "d"}}, false},
        {"a", value_a, false},
        {"e", json{{"value", "e"}}, false},
        {"b", value_b, false},
        {"f", json{{"value", "f"}}, false},
        {"c", value_c, true},
    };

    auto tokens = std::vector<CallToken>{CallToken{"a"}, CallToken{"b"}, CallToken{"c"}};
    auto gatherer = ReplyGatherer{gather_func, nullptr, ctx, tokens};

    EXPECT_CALL(ctx, Reply(json({value_a, value_b, value_c}))).Times(1);

    for (const auto& tup : in_out_tuple) {
        std::string token;
        json reply;
        bool expect;

        std::tie(token, reply, expect) = tup;
        auto finished = gatherer.Gather(token, reply);
        ASSERT_EQ(finished, expect);

        if (finished) {
            // ASSERT_EQ(gathered_replies, json({value_a, value_b, value_c}));
        }
    }
}
*/

/**
 * @brief Test Message unmarshalling and accessors methods.
 */
TEST(iotea, Message) {
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
        const auto m = iotea::core::Message::FromJson(t.raw);

        ASSERT_EQ(t.is_event, m.IsEvent());
        ASSERT_EQ(t.is_discover, m.IsDiscover());
        ASSERT_EQ(t.is_error, m.IsError());
        ASSERT_EQ(t.code, m.GetCode());
    }
}

/**
 * @brief Test DiscoverMessage unmarshalling and accessors methods.
 */
TEST(iotea, DiscoverMessage) {
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
        const auto m = iotea::core::DiscoverMessage::FromJson(t.raw);

        ASSERT_EQ(t.version, m.GetVersion());
        ASSERT_EQ(t.return_topic, m.GetReturnTopic());
    }
}

/**
 * @brief Test ErrorMessage unmarshalling and accessors methods.
 */
TEST(iotea, ErrorMessage) {
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
        const auto m = iotea::core::ErrorMessage::FromJson(t.raw);

        ASSERT_EQ(t.code, m.GetCode());
        ASSERT_EQ(t.message, m.GetMessage());
    }
}

/**
 * @brief Test Event unmarshalling and accessors methods.
 */

TEST(iotea, PlatformEvent) {
    struct {
        PlatformEvent::Type type;
        json data;
        timepoint_t timestamp;

        json raw;
    } tests[] {
        {
            PlatformEvent::Type::TALENT_RULES_SET,
            json::object(),
            iotea::core::timepoint_t{std::chrono::milliseconds{static_cast<int64_t>(12345)}},
            json::parse(R"({
                "type": "platform.talent.config.set",
                "data": {},
                "timestamp": 12345
            })")
        },
        {
            PlatformEvent::Type::TALENT_RULES_UNSET,
            json::object(),
            iotea::core::timepoint_t{std::chrono::milliseconds{static_cast<int64_t>(12345)}},
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
TEST(iotea, Event) {
    auto subject = "test_subject";
    auto feature = "test_feature";
    auto value = json{{"test", "json"}};
    auto type = "test_type";
    auto instance = "test_instance";
    auto return_topic = "test/return/topic";
    auto when = iotea::core::timepoint_t{std::chrono::milliseconds{static_cast<int64_t>(1615209290000)}};

    auto event = iotea::core::Event(subject, feature, value, type, instance, return_topic, when);

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

TEST(iotea, Event_FromJson) {
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

/**
 * @brief Test CallContext and verify that reply messages are correctly formatted.
 */
/* TODO re-think mocking and re-implement
TEST(iotea, CallContext_Reply) {
    // Construct the "function call event" that we should reply to
    auto subject = std::string{"subject"};
    auto feature = std::string{"feature"};
    auto channel_id = std::string{"00000000-0000-0000-0000-000000000000"};
    auto call_id = std::string{"00000000-0000-0000-0000-000000000000"};
    auto type = std::string{"default"};
    auto instance = std::string{"default"};
    auto return_topic = std::string{"return_topic"};
    auto call_value = json{{"chnl", channel_id}, {"call", call_id}, {"args", json{"alpha", "beta"}}};
    auto call_event = Event{subject, feature, call_value, type, instance, return_topic};

    // Construct the CallContext
    auto call_handler = std::make_shared<CallHandler>();
    auto publisher = std::make_shared<iotea::mock::core::Publisher>();
    auto fake_uuid = std::string{"00000000-0000-0000-0000-000000000000"};
    auto uuid_gen = [fake_uuid]{ return fake_uuid; };
    auto talent_id = std::string{"talent_id"};
    auto ctx = CallContext{talent_id, channel_id, feature, call_event, call_handler, publisher, uuid_gen};

    // Construct the "reply event"
    auto reply_value = json{"gamma", "delta"};
    auto reply_event = Event{subject, feature, 
        json{{"$tsuffix", "/" + channel_id + "/" + call_id}, {"$vpath", "value"}, {"value", reply_value}}
    };

    // Next we compare what is published with what we expect to be published.
    // This is a bit convoluted because the event contains a timestamp that
    // depends on when the event was created (yes we could dependency inject a
    // clock function) so the textual representation of two otherwise equal
    // event will differ if they where created a different times.
    //
    // Store the textual representation of the published event then unmarhal it
    // and compare it to what we expect using the overerloaded operator==()
    // which doesn't look at the "when" field.
    std::string raw_event;
    EXPECT_CALL(*publisher, Publish(std::string{return_topic}, ::testing::_)).WillOnce(::testing::SaveArg<1>(&raw_event));
    ctx.Reply(reply_value);

    ASSERT_EQ(Event::FromJson(json::parse(raw_event)), reply_event);
}
*/
