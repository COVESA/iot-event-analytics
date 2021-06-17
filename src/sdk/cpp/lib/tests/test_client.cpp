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

#include "client.hpp"

using json = nlohmann::json;

using namespace iotea::core;

const json test_config = {
    {"adapters", {
                     {
                         {"platform", true},
                         {"module", {
                                 {"name", "mqtt_client"},
                            }
                         },
                         {"config", {
                                {"brokerUrl", "mqtt://localhost:1883"},
                                {"topicNamespace", "iotea/"}
                            }
                         }
                     }
                 }
    }};

class TestProtocolGateway : public ProtocolGateway {
   public:
    TestProtocolGateway() : ProtocolGateway{test_config, "", false} {}

    MOCK_METHOD(void, Publish, (const std::string&, const std::string&, const PublishOptions&), (override));

    MOCK_METHOD(void, Subscribe, (const std::string&, on_msg_func_ptr, const SubscribeOptions&), (override));

    MOCK_METHOD(void, SubscribeShared, (const std::string&, const std::string&, on_msg_func_ptr, const SubscribeOptions&), (override));

    MOCK_METHOD(void, Start, (), (override));
    MOCK_METHOD(void, Stop, (), (override));
};

TEST(client, Client_Receive) {
    class TestClient : public Client {
       public:
        TestClient()
            : Client(std::make_shared<TestProtocolGateway>(),
                    std::make_shared<CalleeTalent>("00000000-0000-0000-0000-000000000000"),
                    std::make_shared<ReplyHandler>()) {}

        using Client::Receive;
        MOCK_METHOD(void, HandleEvent, (const std::string&, const std::string&), (override));
        MOCK_METHOD(void, HandleCallReply, (const std::string&, const std::string&, const call_id_t&, const std::string&), (override));
        MOCK_METHOD(void, HandleDiscover, (const std::string&), (override));
        MOCK_METHOD(void, HandlePlatformEvent, (const std::string&), (override));
    };

    TestClient client;

    // Verify that messages sent under the "event topic" get routed to HandleEvent
    EXPECT_CALL(client, HandleEvent("talent-name", "some message"));
    client.Receive("iotea/talent/talent-name/events", "some message", "");

    // Verify that messages sent under the "call reply topic" get routed to HandleCallReply
    EXPECT_CALL(client, HandleCallReply("talent-name", "channel_id", call_id_t{"call_id"}, "some message"));
    client.Receive("iotea/talent/talent-name/events/talent-name.channel_id/call_id", "some message", "");

    // Verify that messages sent under the "discover topic" get routed to HandleDiscover
    EXPECT_CALL(client, HandleDiscover("some message"));
    client.Receive("iotea/configManager/talents/discover", "some message", "");

    // Verify that messages sent under the "platform event topic" get routed to HandlePlatformEvent
    EXPECT_CALL(client, HandlePlatformEvent("some message"));
    client.Receive("iotea/platform/$events", "some message", "");
}

TEST(client, Client_HandleAsCall) {
    class TestClient : public Client {
       public:
        TestClient()
            : Client(std::make_shared<TestProtocolGateway>(),
                    std::make_shared<CalleeTalent>("00000000-0000-0000-0000-000000000000"),
                    std::make_shared<ReplyHandler>()) {}

        using Client::HandleAsCall;
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}
    };

    TestClient client;

    auto alpha = std::make_shared<TestFunctionTalent>("alpha");

    auto beta = std::make_shared<TestFunctionTalent>("beta");
    beta->RegisterFunction("function", [](const json&, call_ctx_ptr){});

    client.RegisterFunctionTalent(alpha);

    auto value = json::parse(R"({
        "args": [1, 2, 3, 4],
        "chnl": "00000000-0000-0000-0000-000000000000",
        "call": "00000000-0000-0000-0000-000000000000",
        "timeoutAtMs": 1234
    })");
    auto event = std::make_shared<Event>("subject", "beta.function-in", value, json{});

    ASSERT_FALSE(client.HandleAsCall(alpha, event));

    client.RegisterFunctionTalent(beta);
    ASSERT_TRUE(client.HandleAsCall(beta, event));
}

TEST(client, Client_HandleEvent) {

    class TestTalent : public Talent {
       public:
        explicit TestTalent(const std::string& name)
            : Talent{name} {}

        MOCK_METHOD(void, OnEvent, (event_ptr, event_ctx_ptr), (override));
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}

        MOCK_METHOD(void, OnEvent, (event_ptr, event_ctx_ptr), (override));
    };

    class TestCalleeTalent : public CalleeTalent {
       public:
        explicit TestCalleeTalent(const std::string& name)
            : CalleeTalent{name} {}

        MOCK_METHOD(void, OnEvent, (event_ptr, event_ctx_ptr), (override));
    };

    class TestClient : public Client {
       public:
        explicit TestClient(std::shared_ptr<CalleeTalent> callee_talent)
            : Client(std::make_shared<TestProtocolGateway>(),
                    callee_talent,
                    std::make_shared<ReplyHandler>()) {}

        MOCK_METHOD(void, HandleError, (error_message_ptr), (override));
        MOCK_METHOD(bool, HandleAsCall, (std::shared_ptr<FunctionTalent>, event_ptr), (override));

        using Client::HandleEvent;
    };

    auto callee_talent = std::make_shared<TestCalleeTalent>("00000000-0000-0000-0000-000000000000");
    TestClient client{callee_talent};

    // Errors should immediately be passed on to HandleError
    //
    EXPECT_CALL(client, HandleError(::testing::_));
    client.HandleEvent("function_talent", R"({
        "msgType": 4,
        "code": 4000
    })");


    auto function_talent = std::make_shared<TestFunctionTalent>("function_talent");

    // We need a function in order to trigger the first branch in HandleEvent
    // where we check if the event is associated with a function talent
    client.RegisterFunctionTalent(function_talent);


    // Expect HandleAsCall to be called when a "function call event" is
    // handled. Handle event should return immediately if the call was handled.
    EXPECT_CALL(client, HandleAsCall(::testing::_, ::testing::_)).WillOnce(::testing::Return(true));
    client.HandleEvent("function_talent", R"({
        "msgType": 1,
        "subject": "subject",
        "feature": "function_talent.function-in",
        "value": {
                "args": [1, 2, 3, 4],
                "chnl": "00000000-0000-0000-0000-000000000000",
                "call": "00000000-0000-0000-0000-000000000000"
            },
        "$features": "Not important for this test",
        "type": "type",
        "instance": "instance",
        "returnTopic": "return_topic",
        "whenMs": 1234
    })");


    // Expect HandleAsCall to be called when a "function call event" is
    // handled.
    EXPECT_CALL(client, HandleAsCall(::testing::_, ::testing::_)).WillOnce(::testing::Return(false));

    // HandleEvent should forward the event to the FunctionTalent's
    // OnEvent method if HandleAsCall returns false.
    EXPECT_CALL(*function_talent, OnEvent(::testing::_, ::testing::_));

    client.HandleEvent("function_talent", R"({
        "msgType": 1,
        "subject": "subject",
        "feature": "feature",
        "$features": "Not important for this test",
        "value": "value",
        "type": "type",
        "instance": "instance",
        "whenMs": 1234
    })");


    // We need a plain talent to test that events are properly forwarded.
    auto subscription_talent = std::make_shared<TestTalent>("subscription_talent");
    client.RegisterTalent(subscription_talent);

    // Verify that the subscription talent's OnEvent methods was called
    EXPECT_CALL(*subscription_talent, OnEvent(::testing::_, ::testing::_));
    client.HandleEvent("subscription_talent", R"({
        "msgType": 1,
        "subject": "subject",
        "feature": "feature",
        "$features": "Not important for this test",
        "value": "value",
        "type": "type",
        "instance": "instance",
        "whenMs": 1234
    })");


    // If event was not aimed at any of the user supplied talents then at least
    // it should be aimed at the interal CalleeTalent
    EXPECT_CALL(*callee_talent, OnEvent(::testing::_, ::testing::_));
    client.HandleEvent("00000000-0000-0000-0000-000000000000", R"({
        "msgType": 1,
        "subject": "subject",
        "feature": "feature",
        "value": "value",
        "$features": "Not important for this test",
        "type": "type",
        "instance": "instance",
        "whenMs": 1234
    })");
}

TEST(client, Client_HandleDiscover) {

    class TestTalent : public Talent {
       public:
        explicit TestTalent(const std::string& name)
            : Talent{name} {}

        MOCK_METHOD(schema::Schema, GetSchema, (), (const override));
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}

        MOCK_METHOD(schema::Schema, GetSchema, (), (const override));
    };

    class TestCalleeTalent : public CalleeTalent {
       public:
        explicit TestCalleeTalent(const std::string& name)
            : CalleeTalent{name} {}

        MOCK_METHOD(schema::Schema, GetSchema, (), (const override));
        MOCK_METHOD(bool, HasSchema, (), (const override));
    };

    class TestClient : public Client {
       public:
        TestClient(std::shared_ptr<TestProtocolGateway> gateway, std::shared_ptr<CalleeTalent> callee_talent)
            : Client(gateway,
                    callee_talent,
                    std::make_shared<ReplyHandler>()) {}

        using Client::HandleDiscover;
    };

    auto gateway = std::make_shared<TestProtocolGateway>();
    auto callee_talent = std::make_shared<TestCalleeTalent>("00000000-0000-0000-0000-000000000000");
    TestClient client{gateway, callee_talent};

    auto function_talent = std::make_shared<TestFunctionTalent>("function_talent");
    client.RegisterFunctionTalent(function_talent);

    auto subscription_talent = std::make_shared<TestTalent>("subscription_talent");
    client.RegisterTalent(subscription_talent);

    EXPECT_CALL(*function_talent, GetSchema())
        .WillRepeatedly(::testing::Return(schema::Schema{"function_talent", {}, {}, std::make_shared<schema::Rule>(nullptr)}));
    EXPECT_CALL(*subscription_talent, GetSchema())
        .WillRepeatedly(::testing::Return(schema::Schema{"subscription_talent", {}, {}, std::make_shared<schema::Rule>(nullptr)}));

    EXPECT_CALL(*callee_talent, GetSchema())
        .WillRepeatedly(::testing::Return(schema::Schema{"00000000-0000-0000-0000-000000000000", {}, {}, std::make_shared<schema::Rule>(nullptr)}));
    EXPECT_CALL(*gateway, Publish(::testing::_, ::testing::_, ::testing::_)).Times(2);


    EXPECT_CALL(*callee_talent, HasSchema()).WillOnce(::testing::Return(false));

    client.HandleDiscover(R"({"msgType":2,"version":"2.0.0","returnTopic":"123456/configManager/talent/discover"})");

    EXPECT_CALL(*callee_talent, HasSchema()).WillOnce(::testing::Return(true));
    EXPECT_CALL(*gateway, Publish(::testing::_, ::testing::_, ::testing::_)).Times(3);

    client.HandleDiscover(R"({"msgType":2,"version":"2.0.0","returnTopic":"123456/configManager/talent/discover"})");
}

TEST(client, Client_HandlePlatformEvent) {

    class TestTalent : public Talent {
       public:
        explicit TestTalent(const std::string& name)
            : Talent{name} {}

        MOCK_METHOD(void, OnPlatformEvent, (platform_event_ptr), (override));
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}

        MOCK_METHOD(void, OnPlatformEvent, (platform_event_ptr), (override));
    };

    class TestClient : public Client {
       public:
        explicit TestClient(std::shared_ptr<TestProtocolGateway> gateway)
            : Client(gateway,
                    std::make_shared<CalleeTalent>(""),
                    std::make_shared<ReplyHandler>()) {}

        using Client::HandlePlatformEvent;
    };

    auto gateway = std::make_shared<TestProtocolGateway>();
    TestClient client{gateway};

    auto function_talent = std::make_shared<TestFunctionTalent>("function_talent");
    client.RegisterFunctionTalent(function_talent);

    auto subscription_talent = std::make_shared<TestTalent>("subscription_talent");
    client.RegisterTalent(subscription_talent);

    EXPECT_CALL(*function_talent, OnPlatformEvent(::testing::_));
    EXPECT_CALL(*subscription_talent, OnPlatformEvent(::testing::_));

    client.HandlePlatformEvent(R"({"type":"platform.talent.config.set","data":{},"timestamp":1234})");
}

TEST(client, Client_HandleError) {

    class TestTalent : public Talent {
       public:
        explicit TestTalent(const std::string& name)
            : Talent{name} {}

        MOCK_METHOD(void, OnError, (error_message_ptr), (override));
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}

        MOCK_METHOD(void, OnError, (error_message_ptr), (override));
    };

    class TestClient : public Client {
       public:
        explicit TestClient(std::shared_ptr<TestProtocolGateway> gateway)
            : Client(gateway,
                    std::make_shared<CalleeTalent>(""),
                    std::make_shared<ReplyHandler>()) {}

        using Client::HandleError;
    };

    auto gateway = std::make_shared<TestProtocolGateway>();
    TestClient client{gateway};

    auto function_talent = std::make_shared<TestFunctionTalent>("function_talent");
    client.RegisterFunctionTalent(function_talent);

    auto subscription_talent = std::make_shared<TestTalent>("subscription_talent");
    client.RegisterTalent(subscription_talent);

    EXPECT_CALL(*function_talent, OnError(::testing::_));
    EXPECT_CALL(*subscription_talent, OnError(::testing::_));

    auto msg = ErrorMessage::FromJson(json::parse(R"({"code": 4000})"));
    client.HandleError(msg);
}

/*
 * TODO figure out how to test that a mocked function is called multiple times
 * with different arguments. Using a series of EXPECT_CALLs with different
 * arguments causes the test framework to only compare input to the mocked
 * function to the last argument passed to EXPECT_CALL.
TEST(client, Client_SubscribeInternal) {
    class TestTalent : public Talent {
       public:
        TestTalent()
            : Talent{""} {}

        MOCK_METHOD(std::string, GetId, (), (const override));
        MOCK_METHOD(std::string, GetChannelId, (), (const override));
    };

    class TestClient : public Client {
       public:
        explicit TestClient(std::shared_ptr<MqttClient> mqtt_client)
            : Client(mqtt_client,
                    std::make_shared<CalleeTalent>(""),
                    std::make_shared<ReplyHandler>(),
                    "iotea") {}

        using Client::SubscribeInternal;
    };

    auto mqtt_client = std::make_shared<TestMqttClient>();
    TestClient client{mqtt_client};

    auto talent = std::make_shared<TestTalent>();
    client.RegisterTalent(talent);

    EXPECT_CALL(*talent, GetId()).WillOnce(::testing::Return("talent-id"));
    EXPECT_CALL(*talent, GetChannelId()).WillOnce(::testing::Return("00000000-0000-0000-0000-000000000000"));

    EXPECT_CALL(*mqtt_client, Subscribe("$share/talent-id/iotea/configManager/talents/discover", 1)).Times(1);
    EXPECT_CALL(*mqtt_client, Subscribe("$share/talent-id/iotea/platform/$events", 1)).Times(1);
    EXPECT_CALL(*mqtt_client, Subscribe("$share/talent-id/iotea/talent/talent-id/events", 1)).Times(1);
    EXPECT_CALL(*mqtt_client, Subscribe("iotea/talent/talent-id/events/000000000-0000-0000-0000-000000000000/+", 1)).Times(1);

    client.SubscribeInternal(talent);
}
*/
