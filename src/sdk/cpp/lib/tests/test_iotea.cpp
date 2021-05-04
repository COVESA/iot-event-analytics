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

    for (const auto& t : tests) {
        auto talent = Talent("test_RegisterCallee");

        talent.Initialize(std::make_shared<ReplyHandler>(), nullptr, t.uuid_gen);

        for (const auto& c : t.callees) {
            auto callee = talent.RegisterCallee(c.GetTalentId(), c.GetFunc(), c.GetType());
            ASSERT_EQ(callee, c);
        }

        auto have = talent.GetRules();

        ASSERT_EQ(have->Json(), t.want);
    }
}

/**
 * @brief Test that outputs added are reflected in the schema.
 */
TEST(iotea, Talent_AddOutput) {
    class TestTalent : public Talent {
       public:
        TestTalent() : Talent{"Talent_AddOutput"} {}

        using Talent::AddOutput;
        using Talent::GetSchema;

        // A Talent must specify at least one trigger rule or callee. This test
        // is not testing whether a proper rule section is generated, this is
        // just to pass an assert.
        schema::rule_ptr OnGetRules() const {
            return IsSet("feature");
        }
    };

    struct {
        std::vector<std::string> features;
        schema::Metadata metadata;
        uuid_generator_func_ptr uuid_gen;
        json want;
    } tests[] {
        {
            { "test_feature1" },
            schema::Metadata{"metadata1"},
            []{ return "00000000-0000-0000-0000-000000000000"; },
            json::parse(R"({"config":{"outputs":{"Talent_AddOutput.test_feature1":{"description":"metadata1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"feature","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"},"scc":[]},"id":"Talent_AddOutput"})")

        },
        {
            { "test_feature1", "test_feature2" },
            schema::Metadata{"metadata1"},
            []{ return "00000000-0000-0000-0000-000000000000"; },
            json::parse(R"({"config":{"outputs":{"Talent_AddOutput.test_feature1":{"description":"metadata1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"Talent_AddOutput.test_feature2":{"description":"metadata1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"feature","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"},"scc":[]},"id":"Talent_AddOutput"})")

        }
    };

    for (const auto& t : tests) {
        auto talent = TestTalent();

        for (const auto& f : t.features) {
            talent.AddOutput(f, t.metadata);
        }

        auto schema = talent.GetSchema();

        ASSERT_EQ(schema.Json(), t.want);
    }
}

/**
 * @brief Verify that Talent::GetSchema produces a proper schema for
 * each permutation of; Talent
 *
 * - has or doesn't have a ruleset
 * - has or doesn' have callee(s)
 *
 * A valid permutation includes at least one ruleset or callee
 */
TEST(iotea, Talent_GetSchema) {
    class TestTalent : public Talent {
       public:
        explicit TestTalent(schema::rule_ptr rules)
            : Talent{"Talent_GetSchema"}
            , test_rules_{rules} {}

        schema::rule_ptr OnGetRules() const override {
            return test_rules_;
        }

        using Talent::GetSchema;
        schema::rule_ptr test_rules_;
    };
    struct {
        std::vector<Callee> callees;
        schema::rule_ptr rules;
        json want;
    } tests[] {
        {
            { Callee{"talent1", "func1", "type1"} },
            nullptr,
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[{"feature":"talent1.func1-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/Talent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0}],"type":"or"},"scc":["default.Talent_GetSchema.func1-out"]},"id":"Talent_GetSchema"})")
        },
        {
            { Callee{"talent1", "func1", "type1"}, Callee{"talent2", "func2", "type2"} },
            nullptr,
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[{"feature":"talent1.func1-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/Talent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"feature":"talent2.func2-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/Talent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0}],"type":"or"},"scc":["default.Talent_GetSchema.func1-out","default.Talent_GetSchema.func2-out"]},"id":"Talent_GetSchema"})")
        },
        {
            {},
            IsSet("alpha"),
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[{"feature":"alpha","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"},"scc":[]},"id":"Talent_GetSchema"})")
        },
        {
            {},
            OrRules(IsSet("alpha"), IsSet("beta")),
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[{"feature":"alpha","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0},{"feature":"beta","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"},"scc":[]},"id":"Talent_GetSchema"})")
        },
        {
            { Callee{"talent1", "func1", "type1"}, Callee{"talent2", "func2", "type2"} },
            OrRules(IsSet("alpha"), IsSet("beta")),
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[{"feature":"talent1.func1-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/Talent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"feature":"talent2.func2-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/Talent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"excludeOn":["default.talent1.func1-out","default.talent2.func2-out"],"rules":[{"excludeOn":null,"rules":[{"feature":"alpha","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0},{"feature":"beta","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"}],"type":"or"}],"type":"or"},"scc":["default.Talent_GetSchema.func1-out","default.Talent_GetSchema.func2-out"]},"id":"Talent_GetSchema"})")
        }
    };

    for (const auto& t : tests) {
        auto talent = TestTalent{t.rules};

        for (const auto& c : t.callees) {
            talent.RegisterCallee(c.GetTalentId(), c.GetFunc(), c.GetType());
        }

        auto schema = talent.GetSchema();
        ASSERT_EQ(schema.Json(), t.want);
    }
}

/**
 * @brief Test that the Talent::GetInputName() functions produce correct input
 * feature names.
 */
TEST(iotea, Talent_GetInputName) {
    auto t = Talent("Talent_GetInputName");

    ASSERT_EQ(t.GetInputName("feature"), std::string{"feature-in"});
    ASSERT_EQ(t.GetInputName("talent_id", "feature"), std::string{"talent_id.feature-in"});
    ASSERT_EQ(t.GetInputName("type", "talent_id", "feature"), std::string{"type.talent_id.feature-in"});
}

/**
 * @brief Test that the Talent::GetOutputName() functions produce correct
 * output feature names.
 */
TEST(iotea, Talent_GetOutputName) {
    auto t = Talent("Talent_GetOutputName");

    ASSERT_EQ(t.GetOutputName("feature"), std::string{"feature-out"});
    ASSERT_EQ(t.GetOutputName("talent_id", "feature"), std::string{"talent_id.feature-out"});
    ASSERT_EQ(t.GetOutputName("type", "talent_id", "feature"), std::string{"type.talent_id.feature-out"});
}

/**
 * @brief Test that a FunctionTalent retains its assigned name.
 */
TEST(iotea, FunctionTalent_GetId) {
  auto talent = FunctionTalent("test_talent");
  ASSERT_STREQ(talent.GetId().c_str(), "test_talent");
}

/**
 * @brief Verify that FunctionTalent::RegisterFunction adds the function and
 * all it's particularities to the FunctionTalent's rules.
 */
TEST(iotea, FunctionTalent_RegisterFunction) {
    class TestFunctionTalent : public FunctionTalent {
       public:
        TestFunctionTalent() : FunctionTalent{"FunctionTalent_RegisterFunction"} {}

        using FunctionTalent::GetRules;
    };

    struct {
        std::vector<std::string> functions;
        json want;
    } tests[] {
        {
            { "alpha" },
            json::parse(R"({"excludeOn":null,"rules":[{"feature":"FunctionTalent_RegisterFunction.alpha-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"alpha","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0}],"type":"or"})")
        },
        {
            { "alpha", "beta" },
            json::parse(R"({"excludeOn":null,"rules":[{"feature":"FunctionTalent_RegisterFunction.beta-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"beta","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0},{"feature":"FunctionTalent_RegisterFunction.alpha-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"alpha","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0}],"type":"or"})")
        }
    };

    auto test_func = [](const json&, call_ctx_ptr) {};
    for (const auto& t : tests) {
        auto talent = TestFunctionTalent{};

        for (const auto& f : t.functions) {
            talent.RegisterFunction(f, test_func);
        }

        auto rules = talent.GetRules();
        ASSERT_EQ(rules->Json(), t.want);
    }
}

/**
 * @brief Verify that FunctionTalent::GetSchema produces a proper schema for
 * each permutation of; FunctionTalent
 *
 * - has or doesn't have a ruleset
 * - has or doesn' have callee(s)
 * - does or doesn't provides function(s)
 *
 * A valid permutation includes at least one of ruleset, callee or function.
 */
TEST(iotea, FunctionTalent_GetSchema) {
    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(schema::rule_ptr rules)
            : FunctionTalent("FunctionTalent_GetSchema")
            , test_rules_{rules} {}

        schema::rule_ptr OnGetRules() const { return test_rules_; }
        using FunctionTalent::GetSchema;
        schema::rule_ptr test_rules_;
    };

    struct {
        schema::rule_ptr rules;
        std::vector<std::string> functions;
        std::vector<Callee> callees;
        json want;
    } tests[] {
        {
            IsSet("feature1"),
            {},
            {},
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[{"excludeOn":null,"rules":[{"feature":"feature1","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"}],"type":"or"},"scc":[]},"id":"FunctionTalent_GetSchema"})")
        },
        {
            nullptr,
            { "function1" },
            {},
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in"]},"id":"FunctionTalent_GetSchema"})")
        },
        {
            nullptr,
            {},
            { Callee{"talent1", "func1", "type1" } },
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[],"type":"or"},"scc":["default.FunctionTalent_GetSchema.func1-out"]},"id":"FunctionTalent_GetSchema"})")
        },
        {
            IsSet("feature1"),
            { "function1" },
            {},
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function1-in"],"rules":[{"feature":"feature1","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in"]},"id":"FunctionTalent_GetSchema"})")
        },
        {
            IsSet("feature1"),
            {},
            { Callee{"talent1", "func1", "type1" } },
            json::parse(R"({"config":{"outputs":{},"rules":{"excludeOn":null,"rules":[{"excludeOn":["default.talent1.func1-out"],"rules":[{"feature":"feature1","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.func1-out"]},"id":"FunctionTalent_GetSchema"})")
        },
        {
            nullptr,
            { "function1" },
            { Callee{"talent1", "func1", "type1" } },
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"talent1.func1-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/FunctionTalent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function1-out"],"rules":[{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0}],"type":"or"}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in","default.FunctionTalent_GetSchema.func1-out"]},"id":"FunctionTalent_GetSchema"})")
        },
        {
            OrRules(IsSet("feature1"), IsSet("feature2")),
            { "function1", "function2" },
            { Callee{"talent1", "func1", "type1" }, Callee{"talent2", "func2", "type2" } },
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function2-in":{"description":"Argument(s) for function function2","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function2-out":{"description":"Result of function function2","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"talent1.func1-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/FunctionTalent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"feature":"talent2.func2-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/FunctionTalent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function2-out","default.FunctionTalent_GetSchema.function1-out"],"rules":[{"feature":"FunctionTalent_GetSchema.function2-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function2","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0},{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"}},"required":["func","args","chnl","call"],"type":"object"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function2-in","default.FunctionTalent_GetSchema.function1-in"],"rules":[{"feature":"feature1","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0},{"feature":"feature2","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"}],"type":"or"}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in","default.FunctionTalent_GetSchema.function2-in","default.FunctionTalent_GetSchema.func1-out","default.FunctionTalent_GetSchema.func2-out"]},"id":"FunctionTalent_GetSchema"})")
        },
    };

    auto test_func = [](const json&, call_ctx_ptr) {};
    for (const auto& t : tests) {
        auto talent = TestFunctionTalent{t.rules};

        for (const auto& f : t.functions) {
            talent.RegisterFunction(f, test_func);
        }

        for (const auto& c : t.callees) {
            talent.RegisterCallee(c.GetTalentId(), c.GetFunc(), c.GetType());
        }


        auto schema = talent.GetSchema();
        EXPECT_EQ(schema.Json(), t.want);
    }

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

/**
 * @brief Verify that Event::FromJson can build an Event object from JSON.
 */
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

/**
 * @brief Verify that ReplyHandler::ExtractGatherer finds, removes and returns
 * the Gatherer associated with a particular call_token_t
 */
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

/**
 * @brief Verify that ReplyHandler::ExtractTimedOut correcly extracts gatherers
 * whose CallTokens have timed out.
 */
TEST(iotea, ReplyHandler_ExtractTimedOut) {
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

/**
 * @brief Verify that Tokenizer::PushBack permitts the caller to push the last
 * token back into the stream.
 */
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

/**
 * @brief Verify that JsonQuery can parse the entire range of query
 * permutations.
 */
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

/**
 * @brief Verify that JsonQuery throws expections as expected.
 */
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

/**
 * @brief Verify that OutgoingCall::Json produces a properly formatted JSON
 * function call event.
 */
TEST(iotea, OutgoingCall) {
    auto c = OutgoingCall{"my_talent_id", "my_channel_id", "my_call_id", "my_func", {{"key", "value"}}, "my_subject", "my_type", 1234};

    auto want = json::parse(R"({
        "feature": "my_talent_id.my_func-in",
        "subject":"my_subject",
        "type":"my_type",
        "value": {
            "args": {"key": "value"},
            "call": "my_call_id",
            "chnl": "my_channel_id",
            "func": "my_func"},
            "whenMs":1234
    })");

    ASSERT_EQ(c.Json(), want);
}

/**
 * @brief Verify that EventContext::Emit sends properly formatted event
 * messages.
 */
TEST(iotea, EventContext_Emit) {
    class TestPublisher : public Publisher {
       public:
        MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    };

    auto publisher = std::make_shared<TestPublisher>();
    auto ctx = EventContext{"my_talent_id", "my_channel_id", "my_subject", "my_return_topic", nullptr, publisher, []{ return ""; }};

    // We can't expect anything for sure with respect to the published data
    // since it's JSON and we can't know a priori which of the different but
    // equivalent orders the object properties are going to be layed out in. So
    // we have to store the output in a string, parse it as JSON and compare it
    // "semantically" to what we expect.
    std::string raw_published_event;
    EXPECT_CALL(*publisher, Publish(::testing::_, ::testing::_)).Times(1).WillOnce(::testing::SaveArg<1>(&raw_published_event));

    ctx.Emit<std::string>("my_feature", "hello world", "my_type", "my_instance");
    auto published_event = json::parse(raw_published_event);

    // The timestamp in the outgoing event is set to the current epoch time in
    // ms, since we don't know what it will be we have to manually override it
    // before the final comparison.
    ASSERT_NO_THROW(published_event["whenMs"].get<int64_t>());
    published_event["whenMs"] = int64_t{1234};

    auto want = json::parse(R"({
        "subject": "my_subject",
        "feature": "my_feature",
        "value": "hello world",
        "type": "my_type",
        "instance": "my_instance",
        "whenMs": 1234
    })");

    ASSERT_EQ(published_event, want);
}

/**
 * @brief Verify that EventContext::Call sends properly formatted function call
 * messages.
 */
TEST(iotea, EventContext_Call) {
    class TestPublisher : public Publisher {
       public:
        MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    };

    auto publisher = std::make_shared<TestPublisher>();
    auto uuid_gen = []{ return "00000000-0000-0000-0000-000000000000"; };
    auto ctx = EventContext{"my_talent_id", "my_channel_id", "my_subject", "my_return_topic", nullptr, publisher, uuid_gen};
    auto callee = Callee{"target_talent_id", "target_func", "target_type"};

    // We can't expect anything for sure with respect to the published data
    // since it's JSON and we can't know a priori which of the different but
    // equivalent orders the object properties are going to be layed out in. So
    // we have to store the output in a string, parse it as JSON and compare it
    // "semantically" to what we expect.
    std::string raw_published_event;
    EXPECT_CALL(*publisher, Publish(::testing::_, ::testing::_)).Times(1).WillOnce(::testing::SaveArg<1>(&raw_published_event));

    // If the argument isn't an array we expect the SDK to wrap the argument in an array
    auto token = ctx.Call(callee, 42);
    ASSERT_EQ(token.GetCallId(), "00000000-0000-0000-0000-000000000000");

    auto published_event = json::parse(raw_published_event);

    // The timestamp in the outgoing event is set to the current epoch time in
    // ms, since we don't know what it will be we have to manually override it
    // before the final comparison.
    ASSERT_NO_THROW(published_event["whenMs"].get<int64_t>());
    published_event["whenMs"] = int64_t{1234};

    auto want = json::parse(R"({
        "subject": "my_subject",
        "feature": "target_talent_id.target_func-in",
        "type": "target_type",
        "value": {
            "func": "target_func",
            "args": [42],
            "call": "00000000-0000-0000-0000-000000000000",
            "chnl": "my_channel_id"
        },
        "whenMs": 1234
    })");

    ASSERT_EQ(published_event, want);
}

class TestMqttClient : public MqttClient {
   public:
    TestMqttClient() : MqttClient() {};

    MOCK_METHOD(void, Run, (), (override));
    MOCK_METHOD(void, Stop, (), (override));

    MOCK_METHOD(void, Publish, (const std::string&, const std::string&), (override));
    MOCK_METHOD(void, Subscribe, (const std::string&, int), (override));
};

TEST(iotea, Client_Receive) {
    class TestClient : public Client {
       public:
        TestClient()
            : Client(std::make_shared<TestMqttClient>(),
                    std::make_shared<CalleeTalent>("00000000-0000-0000-0000-000000000000"),
                    std::make_shared<ReplyHandler>(),
                    "iotea") {}

        using Client::Receive;
        MOCK_METHOD(void, HandleEvent, (const std::string&, const std::string&), (override));
        MOCK_METHOD(void, HandleCallReply, (const std::string&, const std::string&, const call_id_t&, const std::string&), (override));
        MOCK_METHOD(void, HandleDiscover, (const std::string&), (override));
        MOCK_METHOD(void, HandlePlatformEvent, (const std::string&), (override));
    };

    TestClient client;

    // Verify that messages sent under the "event topic" get routed to HandleEvent
    EXPECT_CALL(client, HandleEvent("talent-name", "some message"));
    client.Receive("iotea/talent/talent-name/events", "some message");

    // Verify that messages sent under the "call reply topic" get routed to HandleCallReply
    EXPECT_CALL(client, HandleCallReply("talent-name", "channel_id", call_id_t{"call_id"}, "some message"));
    client.Receive("iotea/talent/talent-name/events/talent-name.channel_id/call_id", "some message");

    // Verify that messages sent under the "discover topic" get routed to HandleDiscover
    EXPECT_CALL(client, HandleDiscover("some message"));
    client.Receive("iotea/configManager/talents/discover", "some message");

    // Verify that messages sent under the "platform event topic" get routed to HandlePlatformEvent
    EXPECT_CALL(client, HandlePlatformEvent("some message"));
    client.Receive("iotea/platform/$events", "some message");
}

TEST(iotea, Client_HandleAsCall) {
    class TestClient : public Client {
       public:
        TestClient()
            : Client(std::make_shared<TestMqttClient>(),
                    std::make_shared<CalleeTalent>("00000000-0000-0000-0000-000000000000"),
                    std::make_shared<ReplyHandler>(),
                    "iotea") {}

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
        "call": "00000000-0000-0000-0000-000000000000"
    })");
    auto event = Event{"subject", "beta.function-in", value};

    ASSERT_FALSE(client.HandleAsCall(alpha, event));

    client.RegisterFunctionTalent(beta);
    ASSERT_TRUE(client.HandleAsCall(beta, event));
}

TEST(iotea, Client_HandleEvent) {

    class TestTalent : public Talent {
       public:
        explicit TestTalent(const std::string& name)
            : Talent{name} {}

        MOCK_METHOD(void, OnEvent, (const Event&, event_ctx_ptr), (override));
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}

        MOCK_METHOD(void, OnEvent, (const Event&, event_ctx_ptr), (override));
    };

    class TestCalleeTalent : public CalleeTalent {
       public:
        explicit TestCalleeTalent(const std::string& name)
            : CalleeTalent{name} {}

        MOCK_METHOD(void, OnEvent, (const Event&, event_ctx_ptr), (override));
    };

    class TestClient : public Client {
       public:
        explicit TestClient(std::shared_ptr<CalleeTalent> callee_talent)
            : Client(std::make_shared<TestMqttClient>(),
                    callee_talent,
                    std::make_shared<ReplyHandler>(),
                    "iotea") {}

        MOCK_METHOD(void, HandleError, (const ErrorMessage&), (override));
        MOCK_METHOD(bool, HandleAsCall, (std::shared_ptr<FunctionTalent>, const Event&), (override));

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
        "type": "type",
        "instance": "instance",
        "whenMs": 1234
    })");
}

TEST(iotea, Client_HandleDiscover) {

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
        TestClient(std::shared_ptr<MqttClient> mqtt_client, std::shared_ptr<CalleeTalent> callee_talent)
            : Client(mqtt_client,
                    callee_talent,
                    std::make_shared<ReplyHandler>(),
                    "iotea") {}

        using Client::HandleDiscover;
    };

    auto mqtt_client = std::make_shared<TestMqttClient>();
    auto callee_talent = std::make_shared<TestCalleeTalent>("00000000-0000-0000-0000-000000000000");
    TestClient client{mqtt_client, callee_talent};

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
    EXPECT_CALL(*mqtt_client, Publish(::testing::_, ::testing::_)).Times(2);


    EXPECT_CALL(*callee_talent, HasSchema()).WillOnce(::testing::Return(false));

    client.HandleDiscover(R"({"msgType":2,"version":"2.0.0","returnTopic":"123456/configManager/talent/discover"})");

    EXPECT_CALL(*callee_talent, HasSchema()).WillOnce(::testing::Return(true));
    EXPECT_CALL(*mqtt_client, Publish(::testing::_, ::testing::_)).Times(3);

    client.HandleDiscover(R"({"msgType":2,"version":"2.0.0","returnTopic":"123456/configManager/talent/discover"})");
}

TEST(iotea, Client_HandlePlatformEvent) {

    class TestTalent : public Talent {
       public:
        explicit TestTalent(const std::string& name)
            : Talent{name} {}

        MOCK_METHOD(void, OnPlatformEvent, (const PlatformEvent&), (override));
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}

        MOCK_METHOD(void, OnPlatformEvent, (const PlatformEvent&), (override));
    };

    class TestClient : public Client {
       public:
        explicit TestClient(std::shared_ptr<MqttClient> mqtt_client)
            : Client(mqtt_client,
                    std::make_shared<CalleeTalent>(""),
                    std::make_shared<ReplyHandler>(),
                    "iotea") {}

        using Client::HandlePlatformEvent;
    };

    auto mqtt_client = std::make_shared<TestMqttClient>();
    TestClient client{mqtt_client};

    auto function_talent = std::make_shared<TestFunctionTalent>("function_talent");
    client.RegisterFunctionTalent(function_talent);

    auto subscription_talent = std::make_shared<TestTalent>("subscription_talent");
    client.RegisterTalent(subscription_talent);

    EXPECT_CALL(*function_talent, OnPlatformEvent(::testing::_));
    EXPECT_CALL(*subscription_talent, OnPlatformEvent(::testing::_));

    client.HandlePlatformEvent(R"({"type":"platform.talent.config.set","data":{},"timestamp":1234})");
}

TEST(iotea, Client_HandleError) {

    class TestTalent : public Talent {
       public:
        explicit TestTalent(const std::string& name)
            : Talent{name} {}

        MOCK_METHOD(void, OnError, (const ErrorMessage&), (override));
    };

    class TestFunctionTalent : public FunctionTalent {
       public:
        explicit TestFunctionTalent(const std::string& name)
            : FunctionTalent{name} {}

        MOCK_METHOD(void, OnError, (const ErrorMessage&), (override));
    };

    class TestClient : public Client {
       public:
        explicit TestClient(std::shared_ptr<MqttClient> mqtt_client)
            : Client(mqtt_client,
                    std::make_shared<CalleeTalent>(""),
                    std::make_shared<ReplyHandler>(),
                    "iotea") {}

        using Client::HandleError;
    };

    auto mqtt_client = std::make_shared<TestMqttClient>();
    TestClient client{mqtt_client};

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
TEST(iotea, Client_SubscribeInternal) {
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
