/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "testsetrunner.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <initializer_list>

#include "iotea.hpp"
#include "logging.hpp"
#include "nlohmann/json.hpp"

//
// TestRunnerTalent
//
TestRunnerTalent::TestRunnerTalent(const std::string& name, iotea::core::publisher_ptr publisher,
                                   std::initializer_list<std::string> test_set_list)
    : Talent{name, publisher} {
    core::log::Info() << "TestRunnerTalent::TestRunnerTalent()";
    for (auto& test_set : test_set_list) {
        auto testTalentId = std::string{TEST_SET_PREFIX} + "-" + test_set;
        test_sets_.push_back(testTalentId);

        auto get_test_info = testTalentId + "." + GET_TEST_INFO_METHOD_NAME;
        auto get_test_info_callee = CreateCallee(testTalentId, GET_TEST_INFO_METHOD_NAME);

        auto prepare_test_set = testTalentId + "." + PREPARE_TEST_SET_METHOD_NAME;
        auto prepare_test_set_callee = CreateCallee(testTalentId, PREPARE_TEST_SET_METHOD_NAME);

        auto run_test = testTalentId + "." + RUN_TEST_METHOD_NAME;
        auto run_test_callee = CreateCallee(testTalentId, RUN_TEST_METHOD_NAME);

        callee_map_.insert({{get_test_info, get_test_info_callee},
                            {prepare_test_set, prepare_test_set_callee},
                            {run_test, run_test_callee}});

        dependencies_.Add(testTalentId);
    }

    auto metadata = core::schema::Metadata("Event to start the integration tests", "ONE",
                                           core::schema::OutputEncoding(core::schema::OutputEncoding::Type::Boolean));
    AddOutput("run-tests", metadata);

    // TODO add dependency things
}

void TestRunnerTalent::Start() {
    core::log::Info() << "TestRunnerTalent::Start()";
    // TODO subscribe to platform events so that we can update the depencies as
    // the are registered with the platform.
}

core::schema::rules_ptr TestRunnerTalent::OnGetRules() const {
    core::log::Info() << "TestRunnerTalent::OnGetRules()";
    return core::OrRules({core::IsSet(GetId() + "." + "test-tests")});
}

void TestRunnerTalent::OnGetTestInfoResult(const json& result, const core::EventContext& context) {
    core::log::Info() << "TestRunnerTalent::OnGetTestInfoResult()";
    (void)result;
    (void)context;
}

void TestRunnerTalent::OnPrepareTestResult(const json& result, const core::EventContext& context) {
    core::log::Info() << "TestRunnerTalent::OnPrepareTestResult()";
    (void)result;
    (void)context;
}

void TestRunnerTalent::OnRunTestResult(const json& result, const core::EventContext& context) {
    core::log::Info() << "TestRunnerTalent::OnTestResult()";
    (void)result;
    (void)context;
}

bool TestRunnerTalent::RunTestSet(core::EventContext context, const std::string& test_set) {
    core::log::Info() << "TestRunnerTalent::RunTestSet()";

    core::log::Info() << "Get Tests for " << test_set;
    auto full_name = GetTestInfoName(test_set);
    auto callee = callee_map_.find(full_name);
    if (callee == callee_map_.end()) {
        core::log::Error() << "Failed to lookup callee " << full_name;
        return false;
    }

    callee->second.Call(
        test_set, context,
        std::bind(&TestRunnerTalent::OnGetTestInfoResult, this, std::placeholders::_1, std::placeholders::_2));

    return false;
}

bool TestRunnerTalent::RunTestSets(core::EventContext context) {
    auto result = true;

    core::log::Info() << "TestRunnerTalent::RunTestSets()";

    for (auto& test_set : test_sets_) {
        core::log::Info() << "Resuilt of " << test_set << "is" << result;
    }

    for (auto test_set : test_sets_) {
        auto test_set_result = RunTestSet(context, test_set);
        core::log::Info() << "Result of " << test_set << " is " << test_set_result;

        if (!test_set_result) {
            result = false;
        }
    }

    return result;
}

void TestRunnerTalent::OnEvent(const core::Event& event, core::EventContext context) {
    core::log::Info() << "TestRunnerTalent::OnEvent()";
    // We don't really care about what kind of event this is. Since we're only
    // subscribe to one event it has to be that event.
    (void)event;

    if (!dependencies_.CheckAll()) {
        context.Reply(dependencies_.Json());
        return;
    }

    RunTestSets(context);

    core::log::Error() << "Can't start tests because of not connected TestSetTalent(s)";
}

std::string TestRunnerTalent::GetTestInfoName(const std::string& test_set) const {
    core::log::Info() << "TestRunnerTalent::GetTestInfoName()";
    return GetId() + "." + test_set + "." + GET_TEST_INFO_METHOD_NAME;
}

std::string TestRunnerTalent::GetTestPrepareName(const std::string& test_set) const {
    core::log::Info() << "TestRunnerTalent::GetTestPrepareName()";
    return GetId() + "." + test_set + "." + PREPARE_TEST_SET_METHOD_NAME;
}

std::string TestRunnerTalent::GetTestRunTestName(const std::string& test_set) const {
    core::log::Info() << "TestRunnerTalent::GetTestRunTestName()";
    return GetId() + "." + test_set + "." + RUN_TEST_METHOD_NAME;
}
