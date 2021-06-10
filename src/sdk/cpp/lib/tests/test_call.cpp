/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <memory>
#include <vector>
#include <utility>
#include <tuple>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "nlohmann/json.hpp"

#include "client.hpp"

using json = nlohmann::json;

using namespace iotea::core;


/**
 * @brief Test that Gatherer handles timeouts.
 */
TEST(call, Gatherer_HasTimedOut) {
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
TEST(call, Gatherer_Wants) {
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
TEST(call, Gatherer_Gather) {
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
TEST(call, SinkGatherer_Gather) {
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
TEST(call, ReplyGatherer_Gather) {
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
 * @brief Verify that ReplyHandler::ExtractGatherer finds, removes and returns
 * the Gatherer associated with a particular call_token_t
 */
TEST(call, ReplyHandler_ExtractGatherer) {
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

/**
 * @brief Verify that ReplyHandler::ExtractTimedOut correcly extracts gatherers
 * whose CallTokens have timed out.
 */
TEST(call, ReplyHandler_ExtractTimedOut) {
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

/**
 * @brief Verify that OutgoingCall::Json produces a properly formatted JSON
 * function call event.
 */
TEST(call, OutgoingCall) {
    auto c = OutgoingCall{"my_talent_id", "my_channel_id", "my_call_id", "my_func", {{"key", "value"}}, "my_subject", "my_type", 1234, 1000};

    auto want = json::parse(R"({
        "feature": "my_talent_id.my_func-in",
        "subject":"my_subject",
        "type":"my_type",
        "value": {
            "args": {"key": "value"},
            "call": "my_call_id",
            "chnl": "my_channel_id",
            "func": "my_func",
            "timeoutAtMs": 2234},
        "whenMs": 1000
    })");

    ASSERT_EQ(c.Json(), want);
}

