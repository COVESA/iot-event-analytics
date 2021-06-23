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
#include <iostream>

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "nlohmann/json.hpp"

#include "testset_talent.hpp"

using json = nlohmann::json;

using iotea::test::TalentDependencies;
using iotea::test::TestResult;
using iotea::test::TestSetInfo;
using iotea::test::TestSetTalent;


class MockProtocolGateway : public iotea::core::ProtocolGateway {
   public:
    MockProtocolGateway() : iotea::core::ProtocolGateway{"my_gateway", true} {}
};

class MockCallContext : public iotea::core::CallContext {
   public:
    MockCallContext(iotea::core::event_ptr event, iotea::core::gateway_ptr gateway)
        : iotea::core::CallContext{"talent_id", "channel_id", "feature", event,
            std::make_shared<iotea::core::ReplyHandler>(), gateway, []{return "";}} {}

    MOCK_METHOD(void, Reply, (const json&), (const override));
};

/**
 * @brief Verify that TestResult produces the expected output.
 */
TEST(testset_talent, TestResult_Json) {
    // All results should be wrapped in arrays
    //
    // Make sure objects are wrapped in an array
    TestResult object{"my_result", {{"meaning", "42"}}, 1234};
    auto object_expect = json::parse(R"({
        "name": "my_result",
        "actual": [{ "meaning": "42" }],
        "duration": 1234
    })");

    ASSERT_EQ(object.Json(), object_expect);

    // Make sure arrays are wrapped in... an array
    TestResult array{"my_result", {"result", "array", 42}, 1234};
    auto array_expect = json::parse(R"({
        "name": "my_result",
        "actual": [["result", "array", 42]],
        "duration": 1234
    })");

    ASSERT_EQ(array.Json(), array_expect);

    // Make sure scalars are wrapped in an array
    TestResult scalar{"my_result", 42, 1234};
    auto scalar_expect= json::parse(R"({
        "name": "my_result",
        "actual": [42],
        "duration": 1234
    })");

    ASSERT_EQ(scalar.Json(), scalar_expect);
}

/**
 * @brief Verify that Test::Json produces the expected output.
 */
TEST(testset_talent, Test_Json) {
    auto func = [](iotea::core::call_ctx_ptr){};

    // Make sure objects are wrapped in an array
    iotea::test::Test object{"my_test", {{"meaning", "42"}}, func, 1234};
    auto object_expect = json::parse(R"({
        "name": "my_test",
        "expectedValue": [{ "meaning": "42" }],
        "timeout": 1234
    })");

    ASSERT_EQ(object.Json(), object_expect);

    // Make sure arrays are wrapped in... an array
    iotea::test::Test array{"my_test", {"result", "array", 42}, func, 1234};
    auto array_expect = json::parse(R"({
        "name": "my_test",
        "expectedValue": [["result", "array", 42]],
        "timeout": 1234
    })");

    ASSERT_EQ(array.Json(), array_expect);

    // Make sure scalars are wrapped in an array
    iotea::test::Test scalar{"my_test", 42, func, 1234};
    auto scalar_expect= json::parse(R"({
        "name": "my_test",
        "expectedValue": [42],
        "timeout": 1234
    })");

    ASSERT_EQ(scalar.Json(), scalar_expect);
}

/**
 * @brief Verify that Test::Run calls the embedded function.
 */
TEST(testset_talent, Test_Run) {
    auto called = false;
    auto func = [&called](iotea::core::call_ctx_ptr){ called = true; };

    // Make sure objects are wrapped in an array
    iotea::test::Test test{"my_test", 0, func, 1234};
    test.Run(nullptr);

    ASSERT_TRUE(called);
}

/**
 * @breif Verfiy that tests that are add show up when the TestSetInfo is
 * marshalled to JSON
 */
TEST(testset_talent, TestSetInfo_AddTest) {
    TestSetInfo info{"my_test"};

    auto func = [](iotea::core::call_ctx_ptr){};

    info.AddTest("test1", "expcted_value1", func, 1);
    info.AddTest("test2", "expcted_value2", func, 2);
    info.AddTest("test3", "expcted_value3", func, 3);

    auto expect = json::parse(R"({
        "name": "my_test",
        "tests": [
            {
                "name": "test1",
                "expectedValue": ["expcted_value1"],
                "timeout": 1
            },
            {
                "name": "test2",
                "expectedValue": ["expcted_value2"],
                "timeout": 2
            },
            {
                "name": "test3",
                "expectedValue": ["expcted_value3"],
                "timeout": 3
            }
        ]
    })");

    ASSERT_EQ(info.Json(), expect);
}

TEST(testset_talent, TestSetInfo_RunTest) {
    TestSetInfo info{"my_test"};

    auto call_value = json{{"chnl", "caller_channel_id"}, {"call", "caller_call_id"}, {"timeoutAtMs", 0}};
    auto features = json{};
    auto event = std::make_shared<iotea::core::Event>("subject", "feature", call_value, features, "default", "default", "return_topic", 0);
    auto gw = std::make_shared<MockProtocolGateway>();

    {
        auto ctx = std::make_shared<MockCallContext>(event, gw);

        // Attempting to run a test that hasn't been registered should report "TEST_ERROR"
        EXPECT_CALL(*ctx, Reply(TestResult{"no_such_test", "TEST_ERROR", -1}.Json()));
        info.RunTest("no_such_test", ctx);
    }

    {
        auto ctx = std::make_shared<MockCallContext>(event, gw);

        // Register and run a test
        auto func = [](iotea::core::call_ctx_ptr ctx){
            ctx->Reply("expected_value");
        };
        info.AddTest("test", "expcted_value", func, 1);
        json expect = "expected_value";
        EXPECT_CALL(*ctx, Reply(expect));
        info.RunTest("test", ctx);
    }
}

/**
 * @brief Verify that TalentDependencies updates its dependencies and reports
 * whether a single or all dependencies are met.
 */
TEST(testset_talent, TalentDependencies) {
    using iotea::core::PlatformEvent;

    auto create_platform_event = [](const std::string& name, bool is_set) {
        return std::make_shared<PlatformEvent>(
            is_set ? PlatformEvent::Type::TALENT_RULES_SET : PlatformEvent::Type::TALENT_RULES_UNSET,
            json{{"talent", name}},
            0);
    };

    TalentDependencies dep;

    dep.Add("alpha");
    dep.Add("beta");

    // Before TalentDependencies has received the proper platform events
    // TalentDependencies::Check should return false for all dependencies.
    ASSERT_FALSE(dep.Check("alpha"));
    ASSERT_FALSE(dep.Check("beta"));
    ASSERT_FALSE(dep.CheckAll());

    // "alpha" comes online
    dep.Update(create_platform_event("alpha", true));
    ASSERT_TRUE(dep.Check("alpha"));
    ASSERT_FALSE(dep.Check("beta"));
    ASSERT_FALSE(dep.CheckAll());

    // "beta" comes online
    dep.Update(create_platform_event("beta", true));
    ASSERT_TRUE(dep.Check("alpha"));
    ASSERT_TRUE(dep.Check("beta"));
    ASSERT_TRUE(dep.CheckAll());

    // "beta" goes offline
    dep.Update(create_platform_event("beta", false));
    ASSERT_TRUE(dep.Check("alpha"));
    ASSERT_FALSE(dep.Check("beta"));
    ASSERT_FALSE(dep.CheckAll());

    // "alpha" goes offline
    dep.Update(create_platform_event("alpha", false));
    ASSERT_FALSE(dep.Check("alpha"));
    ASSERT_FALSE(dep.Check("beta"));
    ASSERT_FALSE(dep.CheckAll());
}

TEST(testset_talent, TestSetTalent_Schema) {
    using iotea::core::Callee;

    TestSetTalent testset{"my_test_set"};

    auto expect_schema = json::parse(R"({"config":{"outputs":{"my_test_set.getTestSetInfo-in":{"description":"Argument(s) for function getTestSetInfo","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"my_test_set.getTestSetInfo-out":{"description":"Result of function getTestSetInfo","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"},"my_test_set.prepare-in":{"description":"Argument(s) for function prepare","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"my_test_set.prepare-out":{"description":"Result of function prepare","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"},"my_test_set.runTest-in":{"description":"Argument(s) for function runTest","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"my_test_set.runTest-out":{"description":"Result of function runTest","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"my_test_set.runTest-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"runTest","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0},{"feature":"my_test_set.getTestSetInfo-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"getTestSetInfo","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0},{"feature":"my_test_set.prepare-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"prepare","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0}],"type":"or"},"scc":["default.my_test_set.prepare-in","default.my_test_set.getTestSetInfo-in","default.my_test_set.runTest-in"]},"id":"my_test_set"})");
    ASSERT_EQ(testset.GetSchema().Json(), expect_schema);
}

TEST(testset_talent, TestSetTalent_Prepare) {
    using iotea::core::Callee;

    TestSetTalent testset{"my_test_set"};

    auto call_value = json{{"chnl", "caller_channel_id"}, {"call", "caller_call_id"}, {"timeoutAtMs", 0}};
    auto features = json{};
    auto event = std::make_shared<iotea::core::Event>("subject", "feature", call_value, features, "default", "default", "return_topic", 0);
    auto gw = std::make_shared<MockProtocolGateway>();
    auto ctx = std::make_shared<MockCallContext>(event, gw);

    EXPECT_CALL(*ctx, Reply(::testing::_));
    testset.Prepare(nullptr, ctx);
}

TEST(testset_talent, TestSetTalent_GetInfo) {
    using iotea::core::Callee;

    TestSetTalent testset{"my_test_set"};

    testset.RegisterTest("alpha", json{"alpha"}, Callee{"alpha_talent", "alpha_func", "alpha_type"}, json{"alpha_args"}, 0);
    testset.RegisterTest("beta", json{"beta"}, Callee{"beta_talent", "beta_func", "beta_type"}, json{"beta_args"}, 0);

    auto call_value = json{{"chnl", "caller_channel_id"}, {"call", "caller_call_id"}, {"timeoutAtMs", 0}};
    auto features = json{};
    auto event = std::make_shared<iotea::core::Event>("subject", "feature", call_value, features, "default", "default", "return_topic", 0);
    auto gw = std::make_shared<MockProtocolGateway>();
    auto ctx = std::make_shared<MockCallContext>(event, gw);

    auto expect = json::parse(R"({
          "name": "my_test_set",
          "tests": [
            {
              "expectedValue": [
                [
                  "alpha"
                ]
              ],
              "name": "alpha",
              "timeout": 0
            },
            {
              "expectedValue": [
                [
                  "beta"
                ]
              ],
              "name": "beta",
              "timeout": 0
            }
          ]
        })");
    EXPECT_CALL(*ctx, Reply(expect));
    testset.GetInfo(nullptr, ctx);
}
