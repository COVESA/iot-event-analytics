/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#include <csignal>
#include <iostream>
#include <memory>

#include "nlohmann/json.hpp"
#include "client.hpp"
#include "protocol_gateway.hpp"
#include "talent_test.hpp"

using json = nlohmann::json;

using iotea::core::Client;
using iotea::core::ProtocolGateway;
using iotea::test::TestSetTalent;

static constexpr char SERVER_ADDRESS[] = "tcp://localhost:1883";
static constexpr char TALENT_NAME[] = "testSet-sdk-cpp";
static constexpr char FEATURE_TESTABLE_TALENT[] = "functionProvider-cpp";
static constexpr char FUNC_TESTABLE_TALENT_ECHO[] = "echo";

class TestSetSDK : public TestSetTalent {
   public:
    TestSetSDK()
        : TestSetTalent(TALENT_NAME) {
        auto callee = RegisterCallee(FEATURE_TESTABLE_TALENT, FUNC_TESTABLE_TALENT_ECHO);

        auto timeout = 500;

        RegisterTest("echoString", "Hello World", callee, {"Hello World"}, timeout);
        RegisterTest("echoBoolean", true, callee, {true}, timeout);
        RegisterTest("echoInteger", 123, callee, {123}, timeout);
        RegisterTest("echoDouble", 123.456, callee, {123.456}, timeout);
        RegisterTest("echoEmptyList", json::array(), callee, json::array({json::array()}), timeout);
        RegisterTest("echoIntegerList", {1, 2, 3}, callee, {{1, 2, 3}}, timeout);
        RegisterTest("echoMixedList", {1, "Hello World", 3.21}, callee, {{1, "Hello World", 3.21}}, timeout);
        RegisterTest("echoDeepList", {1, {2, {3, {4, {5}}}}}, callee, {{1, {2, {3, {4, {5}}}}}}, timeout);
    }
};

static auto mqtt_config = json{
    {"platform", true},
    {"module", {
                   {"name", "./testset_sdk/cpp/build/iotea-sdk-cpp-lib/adapters/mqtt/libmqtt_protocol_adapter.so"}
               }
    },
    {"config",
        {
            {"brokerUrl", "tcp://localhost:1883"},
            {"topicNamespace", "iotea/"}
        }
    }
};
static auto gateway_config = ProtocolGateway::CreateConfig(json{mqtt_config});
static auto gateway = std::make_shared<ProtocolGateway>(gateway_config);
static Client client{gateway};

void signal_handler(int) {
    std::cout << "Stopping..." << std::endl;
    client.Stop();
}


int main(int, char**) {
    auto talent = std::make_shared<TestSetSDK>();

    client.RegisterFunctionTalent(talent);

    std::signal(SIGINT, signal_handler);

    client.Start();

    return 0;
}
