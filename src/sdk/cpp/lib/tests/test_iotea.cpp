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
        TestTalent(const std::string& talent_id, std::shared_ptr<Publisher> publisher)
            : Talent{talent_id, publisher} {}
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
                : TestTalent{"test_OnGetRules", publisher}
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


TEST(iotea, Event) {
    auto subject = "test_subject";
    auto feature = "test_feature";
    auto value = json{{"test", "json"}};
    auto type = "test_type";
    auto instance = "test_instance";
    auto return_topic = "test/return/topic";
    auto when = iotea::core::timepoint_t{std::chrono::milliseconds{static_cast<int64_t>(1615209290000)}};

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
    // The return topic is dropped during serialization because it
    // serialization occurs when events are sent as replies.
    ASSERT_EQ(beta.GetWhen(), when);
}
