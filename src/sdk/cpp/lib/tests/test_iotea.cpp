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
#include <iostream>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include "iotea.hpp"
#include "schema.hpp"
#include "iotea_mocks.h"
#include "jsonquery.hpp"

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

        talent.Initialize(std::make_shared<ReplyHandler>(), nullptr, t.uuid_gen);

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

        void OnEvent(const Event& event, event_ctx_ptr ctx) override {
            auto token = ctx->Call(callee_, event.GetValue());

            auto reply_handler = [this](const json& reply) {
                reply_ = reply;
            };
            ctx->Gather(reply_handler, nullptr, token);
        }
    };

    // Create and set up the talent
    std::string talent_id{"test_Talent"};
    auto talent = TestTalent{talent_id};
    auto reply_handler = std::make_shared<ReplyHandler>();

    std::string fake_uuid{"00000000-0000-0000-0000-000000000000"};
    auto uuid_gen = [fake_uuid]{ return fake_uuid; };
    talent.Initialize(reply_handler, nullptr, uuid_gen);

    // Create what's needed to send an event to the Talent
    auto publisher = std::make_shared<testing::NiceMock<iotea::mock::core::Publisher>>();
    auto ctx = std::make_shared<EventContext>("talent_id", "channel_id", "subject",
        "ingestion/events", reply_handler, publisher, uuid_gen);

    auto value = json{"hello", "world"};
    auto event = Event{"subject", "feature", value};

    // The following sequence of events follow
    // 1. Post the event to the talent
    // 2. Talent calls function and gathers (waits for) the reply
    // 3. Generate a reply send it to the ReplyHandler
    // 4. The ReplyHandler gathers the reply and sends it to the talent which
    //    assigns the reply the member "reply_"
    // 5. The talent's reply_ member is compared to the sent reply

    // 1 & 2
    talent.OnEvent(event, ctx);

    // 3 & 4
    auto gatherer = reply_handler->ExtractGatherer(fake_uuid);
    ASSERT_TRUE(gatherer);

    ASSERT_TRUE(gatherer->Gather(fake_uuid, value));
    auto replies = gatherer->GetReplies();
    gatherer->ForwardReplies(replies);


    // The replies passed to the talent's handler function are packed into a
    // JSON array in the order of the corresponding call tokens.
    auto expect = json::array({value}); // Expect one reply, array with one element
    ASSERT_EQ(talent.reply_, expect);
}

/**
 * @brief Test that Gatherer handles timeouts.
 */
TEST(iotea, Gatherer_HasTimedOut) {
    class TestGatherer : public Gatherer {
       public:
        TestGatherer(const std::vector<CallToken>& tokens, int64_t now_ms)
            : Gatherer(nullptr, tokens, now_ms) {}

        void ForwardReplies(const std::vector<json>&) const override {}
    };

    struct {
        std::vector<CallToken> tokens;
        // The current time and the expected result
        std::vector<std::pair<int64_t, bool>> in_out_pairs;
    } tests[] {
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
    };

    for (const auto& t : tests) {
        auto gatherer = TestGatherer{t.tokens, 0};

        for (const auto& iop : t.in_out_pairs) {
            auto have = gatherer.HasTimedOut(iop.first);
            ASSERT_EQ(have, iop.second);
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
        explicit TestGatherer(const std::vector<CallToken>& tokens)
            : Gatherer(nullptr, tokens) {}

        void ForwardReplies(const std::vector<json>&) const override {}
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
        auto gatherer = TestGatherer{t.tokens};

        for (const auto& iop : t.in_out_pairs) {
            auto have = gatherer.Wants(iop.first);
            ASSERT_EQ(have, iop.second);
        }
    }
}

/**
 * @brief Test that Gatherer only collects replies corresponding to the call
 * IDs it depends on. Verify that replies are stored correctly and that
 * Gatherer::Gather only returns true once all the IDs have been gathered.
 */
TEST(iotea, Gatherer_Gather) {
    class TestGatherer : public Gatherer {
       public:
        explicit TestGatherer(const std::vector<CallToken>& tokens)
            : Gatherer{nullptr, tokens} {}

        void ForwardReplies(const std::vector<json>&) const override {}
    };

    // The tokens the gatherer depends on
    auto tokens = std::vector<CallToken>{CallToken{"a"}, CallToken{"b"}, CallToken{"c"}};
    auto gatherer = TestGatherer{tokens};

    auto value_a = json{{"value", "a"}};
    auto value_b = json{{"value", "b"}};
    auto value_c = json{{"value", "c"}};
    auto in_out_tuple = std::vector<std::tuple<std::string, json, bool>>{
        {"d", json{{"value", "d"}}, false}, // Not a dependency
        {"a", value_a, false},              // First dependency
        {"e", json{{"value", "e"}}, false}, // Not a dependency
        {"b", value_b, false},              // Second dependency
        {"f", json{{"value", "f"}}, false}, // Not a dependency
        {"c", value_c, true},               // Last dependency, expect Gather to return true
    };

    auto expect_replies = std::vector<json>{value_a, value_b, value_c};
    for (const auto& tup : in_out_tuple) {
        std::string token;
        json reply;
        bool expect_done_gathering;

        std::tie(token, reply, expect_done_gathering) = tup;
        auto done_gathering = gatherer.Gather(token, reply);
        ASSERT_EQ(done_gathering, expect_done_gathering);

        if (done_gathering) {
            // Verify that the replies are gathered and gathered in order
            ASSERT_EQ(gatherer.GetReplies(), expect_replies);
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

    auto in_out_tuple = std::vector<std::tuple<std::string, json, bool>>{
        {"a", json{{"value", "a"}}, false},
        {"b", json{{"value", "b"}}, false},
        {"c", json{{"value", "c"}}, true},
    };

    std::vector<json> gathered_replies;
    auto gather_func = [&gathered_replies](std::vector<json> replies) { gathered_replies = replies; };
    auto gatherer = SinkGatherer{gather_func, nullptr, tokens};

    for (const auto& tup : in_out_tuple) {
        std::string token;
        json reply;
        bool expect_done_gathering;

        std::tie(token, reply, expect_done_gathering) = tup;
        auto done_gathering = gatherer.Gather(token, reply);
        ASSERT_EQ(done_gathering, expect_done_gathering);

        if (done_gathering) {
            auto replies = gatherer.GetReplies();
            gatherer.ForwardReplies(replies);
            ASSERT_EQ(replies, gathered_replies);
        }
    }
}

/**
 * @brief Test that ReplyGatherer only collects replies corresponding to the
 * call IDs it depends on. Verify that replies are stored correctly and that
 * the supplied callback function is invoked and returns as expected when all
 * the call IDs have been gathered.
 */
TEST(iotea, ReplyGatherer_Gather) {
    class PublisherMock : public Publisher {
       public:
        MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    };

    auto tokens = std::vector<CallToken>{CallToken{"a"}, CallToken{"b"}, CallToken{"c"}};

    auto in_out_tuple = std::vector<std::tuple<std::string, json, bool>>{
        {"a", json{{"value", "a"}}, false},
        {"b", json{{"value", "b"}}, false},
        {"c", json{{"value", "c"}}, true},
    };

    auto gather_func = [](std::vector<json>) { return json{nullptr}; };
    auto publisher = std::make_shared<PublisherMock>();
    auto replier = PreparedFunctionReply{"talent_id", "feature", "subject", "channel_id", "call_id", "return_topic", publisher};
    auto gatherer = ReplyGatherer{gather_func, nullptr, replier, tokens};

    for (const auto& tup : in_out_tuple) {
        std::string token;
        json reply;
        bool expect_done_gathering;

        std::tie(token, reply, expect_done_gathering) = tup;
        auto done_gathering = gatherer.Gather(token, reply);
        ASSERT_EQ(done_gathering, expect_done_gathering);

        if (done_gathering) {
            auto replies = gatherer.GetReplies();

            EXPECT_CALL(*publisher, Publish(::testing::_, ::testing::_));
            gatherer.ForwardReplies(replies);
        }
    }
}

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
TEST(iotea, Event) {
    auto subject = "test_subject";
    auto feature = "test_feature";
    auto value = json{{"test", "json"}};
    auto type = "test_type";
    auto instance = "test_instance";
    auto return_topic = "test/return/topic";
    auto when = int64_t{1615209290000};

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
TEST(iotea, CallContext_Reply) {
    class PublisherMock : public Publisher {
       public:
        MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    };

    class TestCallContext : public CallContext {
       public:
        explicit TestCallContext(const Event& event, publisher_ptr publisher)
            : CallContext{"talent_id", "channel_id", "feature", event, std::make_shared<ReplyHandler>(), publisher, []{return "";}} {}

        int64_t GetNowMs() const override { return 0; }
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

TEST(iotea, ReplyHandler_ExtractGatherer) {
    class GathererMock : public Gatherer {
       public:
        GathererMock() : Gatherer(nullptr, {}) {}

        MOCK_METHOD(bool, Wants, (const call_id_t&), (const override));
        MOCK_METHOD(void, ForwardReplies, (const std::vector<json>&), (const override));
    };

    ReplyHandler h;

    auto g0 = std::make_shared<GathererMock>();
    EXPECT_CALL(*g0, Wants("g1")).WillRepeatedly(::testing::Return(false));
    EXPECT_CALL(*g0, Wants("g0")).WillOnce(::testing::Return(true));
    h.AddGatherer(g0);

    auto g1 = std::make_shared<GathererMock>();
    EXPECT_CALL(*g1, Wants("g0")).WillRepeatedly(::testing::Return(false));
    EXPECT_CALL(*g1, Wants("g1")).WillOnce(::testing::Return(true));
    h.AddGatherer(g1);

    ASSERT_NE(h.ExtractGatherer("g0"), nullptr);
    ASSERT_EQ(h.ExtractGatherer("g0"), nullptr);

    ASSERT_NE(h.ExtractGatherer("g1"), nullptr);
    ASSERT_EQ(h.ExtractGatherer("g1"), nullptr);
}

TEST(iotea, ReplyHandler_UpdateTime) {
    class GathererMock : public Gatherer {
       public:
        GathererMock() : Gatherer(nullptr, {}) {}

        MOCK_METHOD(bool, HasTimedOut, (int64_t), (const override));
        MOCK_METHOD(void, ForwardReplies, (const std::vector<json>&), (const override));
    };

    ReplyHandler h;

    // Expected to timeout at t=3
    auto g0 = std::make_shared<GathererMock>();
    EXPECT_CALL(*g0, HasTimedOut(0)).WillOnce(::testing::Return(false));
    EXPECT_CALL(*g0, HasTimedOut(1)).WillOnce(::testing::Return(false));
    EXPECT_CALL(*g0, HasTimedOut(2)).WillOnce(::testing::Return(false));
    EXPECT_CALL(*g0, HasTimedOut(3)).WillOnce(::testing::Return(true));
    h.AddGatherer(g0);

    // Expected to timeout at t=2
    auto g1 = std::make_shared<GathererMock>();
    EXPECT_CALL(*g1, HasTimedOut(0)).WillOnce(::testing::Return(false));
    EXPECT_CALL(*g1, HasTimedOut(1)).WillOnce(::testing::Return(false));
    EXPECT_CALL(*g1, HasTimedOut(2)).WillOnce(::testing::Return(true));
    h.AddGatherer(g1);

    // Expected to timeout at t=2
    auto g2 = std::make_shared<GathererMock>();
    EXPECT_CALL(*g2, HasTimedOut(0)).WillOnce(::testing::Return(false));
    EXPECT_CALL(*g2, HasTimedOut(1)).WillOnce(::testing::Return(true));
    h.AddGatherer(g2);

    // Expected to timeout at t=0
    auto g3 = std::make_shared<GathererMock>();
    EXPECT_CALL(*g3, HasTimedOut(0)).WillOnce(::testing::Return(false));
    EXPECT_CALL(*g3, HasTimedOut(1)).WillOnce(::testing::Return(true));
    h.AddGatherer(g3);

    // Expect nothing to timeout at t=0 and t=4
    ASSERT_EQ(h.ExtractTimedOut(0), std::vector<std::shared_ptr<Gatherer>>{});
    ASSERT_EQ(h.ExtractTimedOut(1), (std::vector<std::shared_ptr<Gatherer>>{g2, g3}));
    ASSERT_EQ(h.ExtractTimedOut(2), std::vector<std::shared_ptr<Gatherer>>{g1});
    ASSERT_EQ(h.ExtractTimedOut(3), std::vector<std::shared_ptr<Gatherer>>{g0});
    ASSERT_EQ(h.ExtractTimedOut(4), std::vector<std::shared_ptr<Gatherer>>{});
}

TEST(iotea, Tokenizer_Next) {
    const auto query = "alpha.*.beta[1:2]:label";
    auto t = Tokenizer{query, ".*[]:'"};

    // Verify that all the tokens and nothing but the tokens are returned.
    auto tokens = std::vector<std::string>{ "alpha", ".", "*", ".", "beta", "[", "1", ":", "2", "]", ":", "label" };

    for (const auto& token : tokens) {
        ASSERT_TRUE(t.HasNext());
        ASSERT_EQ(t.Next(), token);
    }
    ASSERT_FALSE(t.HasNext());
}

TEST(iotea, Tokenizer_PushBack) {
    const auto query = "alpha.*.beta[1:2]:label";
    auto t = Tokenizer{query, ".*[]:'"};

    // Verify that all the tokens and nothing but the tokens are returned.
    auto tokens = std::vector<std::string>{ "alpha", ".", "*", ".", "beta", "[", "1", ":", "2", "]", ":", "label" };

    for (const auto& token : tokens) {
        ASSERT_TRUE(t.HasNext());
        ASSERT_EQ(t.Next(), token);
        t.PushBack();
        ASSERT_TRUE(t.HasNext());
        ASSERT_EQ(t.Next(), token);
    }
    ASSERT_FALSE(t.HasNext());
}

TEST(iotea, JsonQuery) {
    struct {
        std::string query;
        std::vector<json> value;
        std::vector<std::vector<QueryResult>> want;
    } tests[] {
        {
            "foo.bar:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")}
                }
            }
        },
        {
            "'foo.bar'.baz:label",
            {
                json::parse(R"({ "foo.bar": { "baz": "qux" } })")
            },
            {
                {
                    QueryResult{"'foo.bar'.baz", "label", json("qux")}
                }
            }
        },
        {
            "foo.*:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })"),
                json::parse(R"({ "foo": { "bar1": "baz1", "bar2": "baz2" } })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")},
                },
                {
                    QueryResult{"foo.bar1", "label", json("baz1")},
                    QueryResult{"foo.bar2", "label", json("baz2")}
                }
            }
        },
        {
            "*.bar:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })"),
                json::parse(R"({ "foo1": { "bar": "baz1" }, "foo2": { "bar": "baz2" } })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")}
                },
                {
                    QueryResult{"foo1.bar", "label", json("baz1")},
                    QueryResult{"foo2.bar", "label", json("baz2")}
                }
            }
        },
        {
            "*.*:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })"),
                json::parse(R"({
                                "foo1": { "bar1": "baz1" },
                                "foo2": { "bar2": "baz2" },
                                "foo3": { "bar3": "baz3" }
                })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")}
                },
                {
                    QueryResult{"foo1.bar1", "label", json("baz1")},
                    QueryResult{"foo2.bar2", "label", json("baz2")},
                    QueryResult{"foo3.bar3", "label", json("baz3")}
                }
            }
        },
        {
            "foo[:][1:3]:label",
            {
                json::parse(R"({ "foo": [
                    [1, 2, 3],
                    [4, 5, 6],
                    [7, 8, 9]
                ] })"),
            },
            {
                {
                    QueryResult{"foo[0][1]", "label", json(2)},
                    QueryResult{"foo[0][2]", "label", json(3)},
                    QueryResult{"foo[1][1]", "label", json(5)},
                    QueryResult{"foo[1][2]", "label", json(6)},
                    QueryResult{"foo[2][1]", "label", json(8)},
                    QueryResult{"foo[2][2]", "label", json(9)},
                }
            }
        },
        {
            "foo.bar[:]:label",
            {
                json::parse(R"({ "foo": { "bar": [] } })"),
                json::parse(R"({ "foo": { "bar": [1] } })"),
                json::parse(R"({ "foo": { "bar": [1, 2, 3] } })")
            },
            {
                {
                },
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                },
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                    QueryResult{"foo.bar[1]", "label", json(2)},
                    QueryResult{"foo.bar[2]", "label", json(3)}
                }
            }
        },
        {
            "foo.bar[0]:label",
            {
                json::parse(R"({ "foo": { "bar": [1] } })"),
                json::parse(R"({ "foo": { "bar": [2, 1] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                },
                {
                    QueryResult{"foo.bar[0]", "label", json(2)},
                }
            }
        },
        {
            "foo.bar[-1]:label",
            {
                json::parse(R"({ "foo": { "bar": [1] } })"),
                json::parse(R"({ "foo": { "bar": [1, 2] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                },
                {
                    QueryResult{"foo.bar[1]", "label", json(2)},
                }
            }
        },
        {
            "foo.bar[0:3]:label",
            {
                json::parse(R"({ "foo": { "bar": [1, 2, 3, 4, 5] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                    QueryResult{"foo.bar[1]", "label", json(2)},
                    QueryResult{"foo.bar[2]", "label", json(3)},
                },
            }
        },
        {
            "foo.bar[2:5]:label",
            {
                json::parse(R"({ "foo": { "bar": [1, 2, 3, 4, 5] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[2]", "label", json(3)},
                    QueryResult{"foo.bar[3]", "label", json(4)},
                    QueryResult{"foo.bar[4]", "label", json(5)},
                },
            }
        },
        {
            "foo.bar.*[1:3].'foo.bar':label",
            {
                json::parse(R"({
                    "foo": {
                        "bar": {
                            "alpha": [
                                {
                                    "foo.bar": 1,
                                    "bar.foo": 2
                                },
                                {
                                    "foo.bar": 3,
                                    "bar.foo": 4
                                },
                                {
                                    "foo.bar": 5,
                                    "bar.foo": 6
                                },
                                {
                                    "foo.bar": 7,
                                    "bar.foo": 8
                                }
                            ],
                            "beta": [
                                {
                                    "foo.bar": 9,
                                    "bar.foo": 10
                                },
                                {
                                    "foo.bar": 11,
                                    "bar.foo": 12
                                },
                                {
                                    "foo.bar": 13,
                                    "bar.foo": 14
                                },
                                {
                                    "foo.bar": 15,
                                    "bar.foo": 16
                                }
                            ]
                        }
                    }
                })")
            },
            {
                {
                    QueryResult{"foo.bar.alpha[1].'foo.bar'", "label", json(3)},
                    QueryResult{"foo.bar.alpha[2].'foo.bar'", "label", json(5)},
                    QueryResult{"foo.bar.beta[1].'foo.bar'", "label", json(11)},
                    QueryResult{"foo.bar.beta[2].'foo.bar'", "label", json(13)}
                }
            }
        }
    };


    for (const auto& t : tests) {
        auto q = JsonQuery{t.query};

        for (size_t i = 0; i < t.value.size(); i++) {
            auto have = q.Query(t.value[i]);
            ASSERT_EQ(have, t.want[i]);
        }
    }
}

TEST(iotea, JsonQuery_Exceptions) {
    auto obj = json::parse(R"({"foo": {"bar": [1, 2, 3, 4], "baz": [1, 2, 3, 4]}})");

    // Key not found
    ASSERT_THROW(JsonQuery("foo.car[0]:label").Query(obj), JsonQueryException);

    // Unterminated range
    ASSERT_THROW(JsonQuery("foo.bar[0:label").Query(obj), JsonQueryException);

    // Invalid range parameter
    ASSERT_THROW(JsonQuery("foo.bar[*]:label").Query(obj), JsonQueryException);
    ASSERT_THROW(JsonQuery("foo.bar[0:*]:label").Query(obj), JsonQueryException);
    ASSERT_THROW(JsonQuery("foo.bar[*:0]:label").Query(obj), JsonQueryException);


    // Invalid range (start is after end)
    ASSERT_THROW(JsonQuery("foo.bar[1:0]:label").Query(obj), JsonQueryException);

    // Out of bounds
    ASSERT_THROW(JsonQuery("foo.bar[100]:label").Query(obj), JsonQueryException);
    ASSERT_THROW(JsonQuery("foo.bar[0:100]:label").Query(obj), JsonQueryException);

    // Invalid query (missing label)
    ASSERT_THROW(JsonQuery("foo.bar[0]").Query(obj), JsonQueryException);
}
