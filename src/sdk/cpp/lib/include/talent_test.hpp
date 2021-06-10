/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_TALENT_TEST_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_TALENT_TEST_HPP_

#include <functional>
#include <unordered_map>

#include "nlohmann/json.hpp"

#include "event.hpp"
#include "call.hpp"
#include "context.hpp"
#include "talent.hpp"

namespace iotea {
namespace test {

class TestResult {
   public:
    TestResult(const std::string& name, const json& actual_value, int32_t duration);

    json Json() const;

   private:
    const std::string name_;
    const json actual_value_;
    const int32_t duration_;
};

class Test {
   public:
    Test(const std::string& name, const json& expected_value, const std::function<void(core::call_ctx_ptr)> func, uint32_t timeout);

    void Run(core::call_ctx_ptr ctx);

    json Json() const;

   private:
    const std::string name_;
    const json expected_value_;
    const std::function<void(core::call_ctx_ptr)> func_;
    const uint32_t timeout_;

};

class TestSetInfo {
   public:
    explicit TestSetInfo(const std::string& name);

    void AddTest(const std::string& name, const json& exepected_value, const std::function<void(core::call_ctx_ptr)>& func, uint32_t timeout);

    void RunTest(const std::string& name, core::call_ctx_ptr ctx);

    std::string GetName() const;

    json Json() const;

   private:
    const std::string name_;
    std::unordered_map<std::string, Test> tests_;
};

class TalentDependencies {
   public:
    void Add(const std::string& talent_id);

    void Update(const core::PlatformEvent& event);

    bool Check(const std::string& talent_id) const;

    bool CheckAll() const;

    json Json() const;

   private:
    std::unordered_map<std::string, bool> dependencies_;
};

class TestSetTalent : public core::FunctionTalent {
   public:
    explicit TestSetTalent(const std::string& name);

    void OnPlatformEvent(const core::PlatformEvent& event) override;

    void RegisterTest(const std::string& name, const json& expect, const core::Callee& callee, const json& args, uint32_t timeout);

   private:
    TestSetInfo test_set_info_;
    TalentDependencies dependencies_;

    void Prepare(const json& args, core::call_ctx_ptr ctx);

    void GetInfo(const json& args, core::call_ctx_ptr ctx);

    void Run(const json& args, core::call_ctx_ptr ctx);
};


} // namespace test
} // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_TALENT_TEST_HPP_
