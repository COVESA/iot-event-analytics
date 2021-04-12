/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef TESTRUNNERTALENT_HPP
#define TESTRUNNERTALENT_HPP

#include <functional>
#include <unordered_map>

#include "iotea.hpp"
#include "nlohmann/json.hpp"

namespace iotea {
namespace test {

class TestRunnerTalent : protected core::Talent {
   private:
    std::vector<std::string> test_sets_;
    std::unordered_map<std::string, core::Callee> callee_map_;
    TalentDependencies dependencies_;

   public:
    TestRunnerTalent(const std::string& name, core::publisher_ptr publisher,
                     std::initializer_list<std::string> test_set_list);

    void Start();

    core::schema::rules_ptr OnGetRules() const override;

    void OnGetTestInfoResult(const json& result, const core::EventContext& context);

    void OnPrepareTestResult(const json& result, const core::EventContext& context);

    void OnRunTestResult(const json& result, const core::EventContext& context);

    bool RunTestSet(core::EventContext context, const std::string& test_set);

    bool RunTestSets(core::EventContext context);

    void OnEvent(const core::Event& event, core::EventContext context) override;

    std::string GetTestInfoName(const std::string& test_set) const;

    std::string GetTestPrepareName(const std::string& test_set) const;

    std::string GetTestRunTestName(const std::string& test_set) const;
};

}  // namespace test
}  // namespace iotea

#endif  // TESTRUNNERTALENT_HPP
