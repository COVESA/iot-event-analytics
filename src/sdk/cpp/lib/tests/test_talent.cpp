#include <memory>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include "iotea.hpp"
#include "schema.hpp"
#include "iotea_mocks.h"

using json = nlohmann::json;

using namespace iotea::core;

class TestTalent : public Talent {
    public:
        explicit TestTalent(std::shared_ptr<Publisher> publisher)
            : Talent{"test_talent", publisher} {}
};

TEST(talent, id_matches_constructed) {
  auto talent = Talent("test_talent", nullptr);
  ASSERT_STREQ(talent.GetId().c_str(), "test_talent");
}

TEST(talent, OnGetRules) {
    class GetRulesTalent : public TestTalent {
        public:
            schema::rules_ptr rules_;

            GetRulesTalent(std::shared_ptr<Publisher> publisher, schema::rules_ptr rules)
                : TestTalent{publisher}
                , rules_{rules} {}

            schema::rules_ptr OnGetRules() const override {
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
                                "type": "or"
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
                                "type": "or"
                            })"),
        },
    };

    for (const auto &t : tests) {
        auto publisher = std::make_shared<iotea::mock::core::Publisher>();
        auto talent = GetRulesTalent(publisher, t.rules);

        auto have = talent.OnGetRules();

        ASSERT_EQ(have ? have->Json() : json(nullptr), t.want);
    }
}
