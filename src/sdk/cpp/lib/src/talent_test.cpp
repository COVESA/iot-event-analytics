/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <vector>
#include <algorithm>
#include <functional>
#include <chrono>

#include "logging.hpp"
#include "talent_test.hpp"

using iotea::core::logging::NamedLogger;

namespace iotea {
namespace test {

static const char GET_TEST_INFO_METHOD_NAME[] = "getTestSetInfo";
static const char PREPARE_TEST_SET_METHOD_NAME[] = "prepare";
static const char RUN_TEST_METHOD_NAME[] = "runTest";
static const char TEST_ERROR[] = "TEST_ERROR";

//
// TestResult
//
TestResult::TestResult(const std::string& name, const json& actual_value, int32_t duration)
    : name_{name}
    , actual_value_{actual_value}
    , duration_{duration} {}

json TestResult::Json() const {
    return json{
        {"name", name_},
        {"actual", actual_value_},
        {"duration", duration_}
    };
}


//
// Test
//
Test::Test(const std::string& name, const json& expected_value, const std::function<void(core::call_ctx_ptr)> func, uint32_t timeout)
    : name_{name}
    , expected_value_{expected_value}
    , func_{func}
    , timeout_{timeout} {}

void Test::Run(core::call_ctx_ptr ctx) {
    func_(ctx);
}

json Test::Json() const {
    return json{
        {"name", name_},
        {"expectedValue", expected_value_},
        {"timeout", timeout_}
    };
}


//
// TestSetInfo
//
static auto logger = NamedLogger("TestSetInfo");

TestSetInfo::TestSetInfo(const std::string& name)
    : name_{name} {}

void TestSetInfo::AddTest(const std::string& name, const json& exepected_value, const std::function<void(core::call_ctx_ptr)>& func, uint32_t timeout) {
    auto test = Test{name, exepected_value, func, timeout};
    tests_.insert({name, test});
}

void TestSetInfo::RunTest(const std::string& name, core::call_ctx_ptr ctx) {
    logger.Info() << "Run Test " << name;

    auto test = tests_.find(name);

    if (test == tests_.end()) {
        logger.Error() << "Test " << name << " has not been registered";

        ctx->Reply(TestResult{name, TEST_ERROR, -1}.Json());
        return;
    }

    test->second.Run(ctx);
}

json TestSetInfo::Json() const {
    json tests = json::array();

    for (auto& test : tests_) {
        tests.push_back(test.second.Json());
    }

    return json{
        {"name", name_},
        {"tests", tests}
    };
}


//
// TalentDependencies
//
void TalentDependencies::Add(const std::string& talent_id) {
    if (dependencies_.count(talent_id) == 0) {
        dependencies_.insert({talent_id, false});
    }
}

bool TalentDependencies::Check(const std::string& talent_id) const {
    auto item = dependencies_.find(talent_id);

    return item == dependencies_.end() ? false : item->second;
}

void TalentDependencies::Update(core::platform_event_ptr event) {
    auto type = event->GetType();
    bool is_set = false;

    switch (type) {
        default:
        // Not our kind of event
        return;
        case core::PlatformEvent::Type::TALENT_RULES_SET:
            is_set = true;
            break;
        case core::PlatformEvent::Type::TALENT_RULES_UNSET:
            is_set = false;
            break;
    }

    auto talent = event->GetData()["talent"].get<std::string>();
    if (dependencies_.find(talent) == dependencies_.end()) {
        // This is not one of our dependencies
        return;
    }

    dependencies_[talent] = is_set;
}

bool TalentDependencies::CheckAll() const {
    for (const auto& dep : dependencies_) {
        if (!dep.second) {
            return false;
        }
    }

    return true;
}

json TalentDependencies::Json() const {
    auto not_connected = json::array();

    for (auto& item : dependencies_) {
        if (!item.second) {
            not_connected.push_back(item.first);
        }
    }

    return json{
        {"result", not_connected.size() != 0},
        {"notConnected", not_connected}
    };
}


//
// TestSetTalent
//
TestSetTalent::TestSetTalent(const std::string& name)
    : core::FunctionTalent{name}
    , test_set_info_{name} {

    RegisterFunction(PREPARE_TEST_SET_METHOD_NAME, [this](const json& args, core::call_ctx_ptr ctx) {
        Prepare(args, ctx);
    });

    RegisterFunction(GET_TEST_INFO_METHOD_NAME, [this](const json& args, core::call_ctx_ptr ctx) {
        GetInfo(args, ctx);
    });

    RegisterFunction(RUN_TEST_METHOD_NAME, [this](const json& args, core::call_ctx_ptr ctx) {
        Run(args, ctx);
    });
}

void TestSetTalent::OnPlatformEvent(core::platform_event_ptr event) {
    dependencies_.Update(event);
}

void TestSetTalent::RegisterTest(const std::string& name, const json& expect, const core::Callee& callee, const json& args, uint32_t timeout) {
    // This is the function that we will delegate to when the runner ask us to
    // run the test called "name"
    //
    auto func = [name, callee, args](core::call_ctx_ptr ctx) {

        auto start = std::chrono::high_resolution_clock::now();
        auto t = ctx->Call(callee, args);

        ctx->GatherAndReply([name, start](std::vector<json> replies) {
                auto stop = std::chrono::high_resolution_clock::now();
                auto delta = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
                auto duration = static_cast<int32_t>(delta);

                return TestResult{name, replies[0], duration}.Json();
            }, nullptr, t);
    };

    dependencies_.Add(callee.GetTalentId());
    test_set_info_.AddTest(name, expect, func, timeout);
}

void TestSetTalent::Prepare(const json&, core::call_ctx_ptr ctx) {
    ctx->Reply(dependencies_.CheckAll());
}

void TestSetTalent::GetInfo(const json&, core::call_ctx_ptr ctx) {
    ctx->Reply(test_set_info_.Json());
}

void TestSetTalent::Run(const json& args, core::call_ctx_ptr ctx) {
    auto test_name = args[0].get<std::string>();
    test_set_info_.RunTest(test_name, ctx);
}

} // namespace test
} // namespace iotea
