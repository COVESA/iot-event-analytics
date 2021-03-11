#include "gtest/gtest.h"

#include <iostream>

#include "nlohmann/json.hpp"
#include "iotea.hpp"

using json = nlohmann::json;

using namespace iotea::core;

/**
 * @brief Builds a JSON object which contains the key/values that we expect a
 * NelsonConstraint to contain.
 *
 * @param feature
 * @param value
 * @param type_selector
 * @param instance_filter
 * @param limit_feature_selection
 * @return json
 */
static json nelson_json_builder(const std::string& feature, schema::NelsonConstraint::Type value,
                         const std::string& type_selector = schema::DEFAULT_TYPE,
                         const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                         const bool limit_feature_selection = true) {
    json j = {
        {"feature", feature},
        {"instanceIdFilter", instance_filter},
        {"limitFeatureSelection", limit_feature_selection},
        {"op", schema::ConstraintType::NELSON},
        {"path", ""},
        {"typeSelector", type_selector},
        {"value", value},
        {"valueType", schema::ValueEncoding::ENCODED}
    };

    return j;
}

/**
 * @brief Create a nelson constraint object. All NelsonContraint constructors
 * take the same parameters.
 *
 * @param type
 * @param feature
 * @param type_selector
 * @param instance_filter
 * @param limit_feature_selection
 * @return schema::rule_ptr
 */
static schema::rule_ptr create_nelson_constraint(schema::NelsonConstraint::Type type, const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                    const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                    const bool limit_feature_selection = true) {
    switch (type) {
        case schema::NelsonConstraint::Type::OUT3_SE:
            return NelsonOut3Se(feature, type_selector, instance_filter, limit_feature_selection);
        case schema::NelsonConstraint::Type::OUT2_SE:
            return NelsonOut2Se(feature, type_selector, instance_filter, limit_feature_selection);
        case schema::NelsonConstraint::Type::OUT1_SE:
            return NelsonOut1Se(feature, type_selector, instance_filter, limit_feature_selection);
        case schema::NelsonConstraint::Type::BIAS:
            return NelsonBias(feature, type_selector, instance_filter, limit_feature_selection);
        case schema::NelsonConstraint::Type::TREND:
            return NelsonTrend(feature, type_selector, instance_filter, limit_feature_selection);
        case schema::NelsonConstraint::Type::ALTER:
            return NelsonAlter(feature, type_selector, instance_filter, limit_feature_selection);
        case schema::NelsonConstraint::Type::LOW_DEV:
            return NelsonLowDev(feature, type_selector, instance_filter, limit_feature_selection);
        case schema::NelsonConstraint::Type::HIGH_DEV:
            return NelsonHighDev(feature, type_selector, instance_filter, limit_feature_selection);
        default:
            assert(0);
    }
}

TEST(Schema, NelsonConstraints) {
    schema::NelsonConstraint::Type types[] = {
        schema::NelsonConstraint::Type::OUT3_SE,
        schema::NelsonConstraint::Type::OUT2_SE,
        schema::NelsonConstraint::Type::OUT1_SE,
        schema::NelsonConstraint::Type::BIAS,
        schema::NelsonConstraint::Type::TREND,
        schema::NelsonConstraint::Type::ALTER,
        schema::NelsonConstraint::Type::LOW_DEV,
        schema::NelsonConstraint::Type::HIGH_DEV
    };

    for (auto type : types) {
        auto have = create_nelson_constraint(type, "test_feature");
        auto want = nelson_json_builder("test_feature", type);
        ASSERT_EQ(have->Json(), want);

        have = create_nelson_constraint(type, "test_feature", "test_type_selector");
        want = nelson_json_builder("test_feature", type, "test_type_selector");
        ASSERT_EQ(have->Json(), want);

        have = create_nelson_constraint(type, "test_feature", "test_type_selector", "test_instance_filter");
        want = nelson_json_builder("test_feature", type, "test_type_selector", "test_instance_filter");
        ASSERT_EQ(have->Json(), want);

        have = create_nelson_constraint(type, "test_feature", "test_type_selector", "test_instance_filter", false);
        want = nelson_json_builder("test_feature", type, "test_type_selector", "test_instance_filter", false);
        ASSERT_EQ(have->Json(), want);
    }
}