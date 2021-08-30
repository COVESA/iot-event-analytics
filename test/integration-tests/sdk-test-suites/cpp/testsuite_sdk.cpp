/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>

#include "nlohmann/json.hpp"
#include "client.hpp"
#include "protocol_gateway.hpp"
#include "testsuite_talent.hpp"

using json = nlohmann::json;

using iotea::core::Client;
using iotea::core::ProtocolGateway;
using iotea::test::TestSuiteTalent;

static const char TALENT_NAME[] = "testSuite-sdk-cpp";
static const char FEATURE_TESTABLE_TALENT[] = "functionProvider-cpp";
static const char FUNC_TESTABLE_TALENT_ECHO[] = "echo";

class TestSuiteSDK : public TestSuiteTalent {
   public:
    TestSuiteSDK()
        : TestSuiteTalent(TALENT_NAME) {
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

std::shared_ptr<Client> client;

void signal_handler(int) {
    if (client) {
        client->Stop();
    }
}

int main(int, char**) {
    std::ifstream file{"../../../config/tests/cpp/config.json"};
    std::string config{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto gateway = std::make_shared<ProtocolGateway>(json::parse(config));
    client = std::make_shared<Client>(gateway);

    auto talent = std::make_shared<TestSuiteSDK>();
    client->RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);
    client->Start();

    return 0;
}
