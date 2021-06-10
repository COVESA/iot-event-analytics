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

#include "talent.hpp"

using json = nlohmann::json;

using namespace iotea::core;


/**
 * @brief Test that a Talent retains its assigned name.
 */
TEST(talent, Talent_GetId) {
  auto talent = Talent("test_talent");
  ASSERT_STREQ(talent.GetId().c_str(), "test_talent");
}

/**
 * @brief Test different rule set permutations and verify that the Talent
 * generates the correspnding JSON schema correctly.
 */
TEST(talent, Talent_OnGetRules) {
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
TEST(talent, Talent_RegisterCallee) {
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
TEST(talent, Talent_AddOutput) {
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
TEST(talent, Talent_GetSchema) {
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
TEST(talent, Talent_GetInputName) {
    auto t = Talent("Talent_GetInputName");

    ASSERT_EQ(t.GetInputName("feature"), std::string{"feature-in"});
    ASSERT_EQ(t.GetInputName("talent_id", "feature"), std::string{"talent_id.feature-in"});
    ASSERT_EQ(t.GetInputName("type", "talent_id", "feature"), std::string{"type.talent_id.feature-in"});
}

/**
 * @brief Test that the Talent::GetOutputName() functions produce correct
 * output feature names.
 */
TEST(talent, Talent_GetOutputName) {
    auto t = Talent("Talent_GetOutputName");

    ASSERT_EQ(t.GetOutputName("feature"), std::string{"feature-out"});
    ASSERT_EQ(t.GetOutputName("talent_id", "feature"), std::string{"talent_id.feature-out"});
    ASSERT_EQ(t.GetOutputName("type", "talent_id", "feature"), std::string{"type.talent_id.feature-out"});
}

/**
 * @brief Test that a FunctionTalent retains its assigned name.
 */
TEST(talent, FunctionTalent_GetId) {
  auto talent = FunctionTalent("test_talent");
  ASSERT_STREQ(talent.GetId().c_str(), "test_talent");
}

/**
 * @brief Verify that FunctionTalent::RegisterFunction adds the function and
 * all it's particularities to the FunctionTalent's rules.
 */
TEST(talent, FunctionTalent_RegisterFunction) {
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
            json::parse(R"({"excludeOn":null,"rules":[{"feature":"FunctionTalent_RegisterFunction.alpha-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"alpha","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0}],"type":"or"})")
        },
        {
            { "alpha", "beta" },
            json::parse(R"({"excludeOn":null,"rules":[{"feature":"FunctionTalent_RegisterFunction.beta-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"beta","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0},{"feature":"FunctionTalent_RegisterFunction.alpha-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"alpha","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0}],"type":"or"})")
        }
    };

    auto test_func = [](const json&, call_ctx_ptr) {};
    for (const auto& t : tests) {
        auto talent = TestFunctionTalent{};

        for (const auto& f : t.functions) {
            talent.RegisterFunction(f, test_func);
        }

        auto rules = talent.GetRules();
        EXPECT_EQ(rules->Json(), t.want);
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
TEST(talent, FunctionTalent_GetSchema) {
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
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in"]},"id":"FunctionTalent_GetSchema"})")
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
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function1-in"],"rules":[{"feature":"feature1","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in"]},"id":"FunctionTalent_GetSchema"})")
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
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"talent1.func1-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/FunctionTalent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function1-out"],"rules":[{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0}],"type":"or"}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in","default.FunctionTalent_GetSchema.func1-out"]},"id":"FunctionTalent_GetSchema"})")
        },
        {
            OrRules(IsSet("feature1"), IsSet("feature2")),
            { "function1", "function2" },
            { Callee{"talent1", "func1", "type1" }, Callee{"talent2", "func2", "type2" } },
            json::parse(R"({"config":{"outputs":{"FunctionTalent_GetSchema.function1-in":{"description":"Argument(s) for function function1","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function1-out":{"description":"Result of function function1","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function2-in":{"description":"Argument(s) for function function2","encoding":{"encoder":null,"type":"object"},"history":0,"ttl":0,"unit":"ONE"},"FunctionTalent_GetSchema.function2-out":{"description":"Result of function function2","encoding":{"encoder":null,"type":"any"},"history":0,"ttl":0,"unit":"ONE"}},"rules":{"excludeOn":null,"rules":[{"feature":"talent1.func1-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/FunctionTalent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"feature":"talent2.func2-out","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"/$tsuffix","typeSelector":"default","value":{"pattern":"^\\/FunctionTalent_GetSchema\\.[^\\/]+\\/.*","type":"string"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function2-out","default.FunctionTalent_GetSchema.function1-out"],"rules":[{"feature":"FunctionTalent_GetSchema.function2-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function2","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0},{"feature":"FunctionTalent_GetSchema.function1-in","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"additionalProperties":false,"properties":{"args":{"type":"array"},"call":{"type":"string"},"chnl":{"type":"string"},"func":{"const":"function1","type":"string"},"timeoutAtMs":{"type":"integer"}},"required":["func","args","chnl","call","timeoutAtMs"],"type":"object"},"valueType":0},{"excludeOn":["default.FunctionTalent_GetSchema.function2-in","default.FunctionTalent_GetSchema.function1-in"],"rules":[{"feature":"feature1","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0},{"feature":"feature2","instanceIdFilter":".*","limitFeatureSelection":true,"op":0,"path":"","typeSelector":"default","value":{"not":{"type":"null"}},"valueType":0}],"type":"or"}],"type":"or"}],"type":"or"},"scc":["default.FunctionTalent_GetSchema.function1-in","default.FunctionTalent_GetSchema.function2-in","default.FunctionTalent_GetSchema.func1-out","default.FunctionTalent_GetSchema.func2-out"]},"id":"FunctionTalent_GetSchema"})")
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
