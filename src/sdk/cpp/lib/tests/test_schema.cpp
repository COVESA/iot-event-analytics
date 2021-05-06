/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <iostream>

#include "gtest/gtest.h"

#include "nlohmann/json.hpp"
#include "iotea.hpp"

using json = nlohmann::json;

using namespace iotea::core;

TEST(Schema, ScalarValueTypes) {
    auto null = schema::NullType{};
    ASSERT_EQ(null.Json(), json::parse(R"({"type": "null"})"));

    auto boolean = schema::BooleanType{};
    ASSERT_EQ(boolean.Json(), json::parse(R"({"type": "boolean"})"));

    auto number = schema::NumberType{};
    ASSERT_EQ(number.Json(), json::parse(R"({"type": "number"})"));

    auto integer = schema::IntegerType{};
    ASSERT_EQ(integer.Json(), json::parse(R"({"type": "integer"})"));
}

TEST(Schema, StringType) {
    // Test constant string values
    struct {
        std::string value;
        json want;
    } constant_tests[] {
        {
            "Hello World",
            json::parse(R"({"type": "string", "const": "Hello World"})")
        },
        {
            R"(["A", "JSON", "array"])",
            json::parse(R"({"type": "string", "const": "[\"A\", \"JSON\", \"array\"]"})"),
        },
        {
            R"({"A": "valid", "JSON": "object"})",
            json::parse(R"({"type": "string", "const": "{\"A\": \"valid\", \"JSON\": \"object\"}"})"),
        },
    };

    for (const auto& t : constant_tests) {
        auto have = schema::StringType{t.value};
        ASSERT_EQ(have.Json(), t.want);
    }


    // Test constrained string values
    struct {
        schema::Opt<unsigned int> min_length;
        schema::Opt<unsigned int> max_length;
        schema::Opt<std::string> pattern;
        json want;
    } constraints_tests[] {
        {
            schema::Opt<unsigned int>{},
            schema::Opt<unsigned int>{},
            schema::Opt<std::string>{},
            json::parse(R"({"type": "string"})"),
        },
        {
            schema::Opt<unsigned int>{1},
            schema::Opt<unsigned int>{},
            schema::Opt<std::string>{},
            json::parse(R"({"type": "string", "min_length": 1})"),
        },
        {
            schema::Opt<unsigned int>(1),
            schema::Opt<unsigned int>(2),
            schema::Opt<std::string>(),
            json::parse(R"({"type": "string", "min_length": 1, "max_length": 2})"),
        },
        {
            schema::Opt<unsigned int>(1),
            schema::Opt<unsigned int>(2),
            schema::Opt<std::string>("a pattern"),
            json::parse(R"({"type": "string", "min_length": 1, "max_length": 2, "pattern": "a pattern"})"),
        }
    };

    for (const auto& t : constraints_tests) {
        auto have = schema::StringType{t.min_length, t.max_length, t.pattern};
        ASSERT_EQ(have.Json(), t.want);
    }

    // Test enumerated string values
    struct {
        std::vector<std::string> enumeration;
        json want;
    } enumeration_tests[] {
        {
            {},
            json::parse(R"({"type": "string", "enum": []})"),
        },
        {
            {"alpha"},
            json::parse(R"({"type": "string", "enum": ["alpha"]})"),
        },
        {
            {"alpha", "beta", "gamma"},
            json::parse(R"({"type": "string", "enum": ["alpha", "beta", "gamma"]})"),
        }
    };

    for (const auto& t : enumeration_tests) {
        auto have = schema::StringType{t.enumeration};
        ASSERT_EQ(have.Json(), t.want);
    }
}

TEST(Schema, ArrayType) {
    struct {
        schema::value_vec items;
        schema::value_ptr contains;
        schema::Opt<unsigned int> min_items;
        schema::Opt<unsigned int> max_items;
        schema::Opt<bool> unique_items;
        schema::Opt<bool> additional_items;

        json want;
    } tests[] {
        // ArrayType with "items"
        {
            {std::make_shared<schema::StringType>("alpha"), std::make_shared<schema::StringType>("beta")},
            {},
            {},
            {},
            {},
            {},
            json::parse(R"({"items":[{"const":"alpha","type":"string"},{"const":"beta","type":"string"}],"type":"array"})"),
        },
        {
            {std::make_shared<schema::StringType>("alpha"), std::make_shared<schema::StringType>("beta")},
            {},
            schema::Opt<unsigned int>{1},
            {},
            {},
            {},
            json::parse(R"({"min_items": 1, "items":[{"const":"alpha","type":"string"},{"const":"beta","type":"string"}],"type":"array"})"),
        },
        {
            {std::make_shared<schema::StringType>("alpha"), std::make_shared<schema::StringType>("beta")},
            {},
            schema::Opt<unsigned int>{1},
            schema::Opt<unsigned int>{2},
            {},
            {},
            json::parse(R"({"max_items": 2, "min_items": 1, "items":[{"const":"alpha","type":"string"},{"const":"beta","type":"string"}],"type":"array"})"),
        },
        {
            {std::make_shared<schema::StringType>("alpha"), std::make_shared<schema::StringType>("beta")},
            {},
            schema::Opt<unsigned int>{1},
            schema::Opt<unsigned int>{2},
            schema::Opt<bool>{true},
            {},
            json::parse(R"({"unique_items": true, "max_items": 2, "min_items": 1, "items":[{"const":"alpha","type":"string"},{"const":"beta","type":"string"}],"type":"array"})"),
        },
        {
            {std::make_shared<schema::StringType>("alpha"), std::make_shared<schema::StringType>("beta")},
            {},
            schema::Opt<unsigned int>{1},
            schema::Opt<unsigned int>{2},
            schema::Opt<bool>{true},
            schema::Opt<bool>{true},
            json::parse(R"({"additional_items": true, "unique_items": true, "max_items": 2, "min_items": 1, "items":[{"const":"alpha","type":"string"},{"const":"beta","type":"string"}],"type":"array"})"),
        },

        // ArrayType with "contains"
        {
            {},
            std::make_shared<schema::StringType>("alpha"),
            {},
            {},
            {},
            {},
            json::parse(R"({"contains":{"const":"alpha","type":"string"},"type":"array"})"),
        },
        {
            {},
            std::make_shared<schema::StringType>("alpha"),
            {},
            {},
            {},
            {},
            json::parse(R"({"contains":{"const":"alpha","type":"string"},"type":"array"})"),
        },
        {
            {},
            std::make_shared<schema::StringType>("alpha"),
            schema::Opt<unsigned int>{1},
            {},
            {},
            {},
            json::parse(R"({"min_items": 1, "contains":{"const":"alpha","type":"string"},"type":"array"})"),
        },
        {
            {},
            std::make_shared<schema::StringType>("alpha"),
            schema::Opt<unsigned int>{1},
            schema::Opt<unsigned int>{2},
            {},
            {},
            json::parse(R"({"max_items": 2, "min_items": 1, "contains":{"const":"alpha","type":"string"},"type":"array"})"),
        },
        {
            {},
            std::make_shared<schema::StringType>("alpha"),
            schema::Opt<unsigned int>{1},
            schema::Opt<unsigned int>{2},
            schema::Opt<bool>{true},
            {},
            json::parse(R"({"unique_items": true, "max_items": 2, "min_items": 1, "contains":{"const":"alpha","type":"string"},"type":"array"})"),
        },
        {
            {},
            std::make_shared<schema::StringType>("alpha"),
            schema::Opt<unsigned int>{1},
            schema::Opt<unsigned int>{2},
            schema::Opt<bool>{true},
            schema::Opt<bool>{true},
            json::parse(R"({"additional_items": true, "unique_items": true, "max_items": 2, "min_items": 1, "contains":{"const":"alpha","type":"string"},"type":"array"})"),
        }
    };

    for (const auto& t : tests) {
        if (t.items.size() > 0) {
            auto have = schema::ArrayType(t.items, t.min_items, t.max_items, t.unique_items, t.additional_items);
            ASSERT_EQ(have.Json(), t.want);
        } else {
            auto have = schema::ArrayType(t.contains, t.min_items, t.max_items, t.unique_items, t.additional_items);
            ASSERT_EQ(have.Json(), t.want);
        }
    }
}

TEST(Schema, Property) {
    struct {
        std::string name;
        schema::value_ptr value;

        json want;
    } tests[] {
        {
            "boolean_property",
            std::make_shared<schema::BooleanType>(),
            json::parse(R"({"boolean_property":{"type":"boolean"}})"),
        },
        {
            "number_property",
            std::make_shared<schema::NumberType>(),
            json::parse(R"({"number_property":{"type":"number"}})"),
        },
    };

    for (const auto& t : tests) {
        auto have = schema::Property(t.name, t.value);
        ASSERT_EQ(have.Json(), t.want);
    }
}

TEST(Schema, Properties) {
    auto p1 = std::make_pair(std::string{"alpha"}, schema::Property{"boolean_property", std::make_shared<schema::BooleanType>()});
    auto p2 = std::make_pair(std::string{"beta"}, schema::Property{"number_property", std::make_shared<schema::NumberType>()});

    auto have = schema::Properties{p1, p2};

    auto want = json::parse(R"({"alpha":{"boolean_property":{"type":"boolean"}},"beta":{"number_property":{"type":"number"}}})");

    ASSERT_EQ(have.Json(), want);
}

TEST(Schema, ObjectType) {
    struct {
        schema::property_map properties;
        std::vector<std::string> required;
        bool additional_properties;
        json want;
    } tests[] {
        {
            {},
            {},
            {},
            json::parse(R"({"additionalProperties":false,"properties":null,"type":"object"})"),
        },
        {
            {
                {std::string{"boolean_property"}, std::make_shared<schema::BooleanType>()},
            },
            {},
            {},
            json::parse(R"({"additionalProperties":false,"properties":{"boolean_property":{"type":"boolean"}},"type":"object"})"),
        },
        {
            {
                {std::string{"boolean_property"}, std::make_shared<schema::BooleanType>()},
            },
            {"boolean_property"},
            false,
            json::parse(R"({"additionalProperties":false,"properties":{"boolean_property":{"type":"boolean"}},"required":["boolean_property"],"type":"object"})"),
        },
        {
            {
                {std::string{"boolean_property"}, std::make_shared<schema::BooleanType>()},
                {std::string{"number_property"}, std::make_shared<schema::NumberType>()}
            },
            {"boolean_property", "number_property"},
            true,

            json::parse(R"({"additionalProperties":true,"properties":{"boolean_property":{"type":"boolean"},"number_property":{"type":"number"}},"required":["boolean_property","number_property"],"type":"object"})"),
        }
    };

    for (const auto& t : tests) {
        auto have = schema::ObjectType{t.properties, t.required, t.additional_properties};
        ASSERT_EQ(have.Json(), t.want);
    }
}


static json constraint_json_builder(const std::string& feature,
        const schema::ConstraintType constraint_type,
        std::shared_ptr<schema::SchemaEntity> value, const std::string& type_selector,
        const schema::ValueEncoding value_encoding, const std::string& path,
        const std::string& instance_filter,
        const bool limit_feature_selection) {
    auto j = json{
        {"feature", feature},
        {"op", constraint_type},
        {"typeSelector", type_selector},
        {"valueType", value_encoding},
        {"value", value ? value->Json() : json(nullptr)},
        {"path", path},
        {"instanceIdFilter", instance_filter},
        {"limitFeatureSelection", limit_feature_selection},
    };

    return j;
}

template <typename T>
static json equals_json_builder(const std::string& feature, const T& value,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = json{{"const", json(value)}};
    return j;
}

TEST(Schema, Equals) {

    auto have = Equals("test_feature", 1234);
    auto want = equals_json_builder("test_feature", 1234);
    ASSERT_EQ(have->Json(), want);

    have = Equals("test_feature", 1234, "type_selector");
    want = equals_json_builder("test_feature", 1234, "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = Equals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    want = equals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = Equals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = equals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = Equals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = equals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = Equals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = equals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


template <typename T>
static json notequals_json_builder(const std::string& feature, const T& value,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = json{{"not", {{"const", json(value)}}}};
    return j;
}

TEST(Schema, NotEquals) {

    auto have = NotEquals("test_feature", 1234);
    auto want = notequals_json_builder("test_feature", 1234);
    ASSERT_EQ(have->Json(), want);

    have = NotEquals("test_feature", 1234, "type_selector");
    want = notequals_json_builder("test_feature", 1234, "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = NotEquals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    want = notequals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = NotEquals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = notequals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = NotEquals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = notequals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = NotEquals("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = notequals_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


template <typename T>
static json lessthan_json_builder(const std::string& feature, const T& value,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = json{{"type", "number"}, {"exclusiveMaximum", json(value)}};
    return j;
}

TEST(Schema, LessThan) {

    auto have = LessThan("test_feature", 1234);
    auto want = lessthan_json_builder("test_feature", 1234);
    ASSERT_EQ(have->Json(), want);

    have = LessThan("test_feature", 1234, "type_selector");
    want = lessthan_json_builder("test_feature", 1234, "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = LessThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    want = lessthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = LessThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = lessthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = LessThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = lessthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = LessThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = lessthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


template <typename T>
static json lessthanorequalto_json_builder(const std::string& feature, const T& value,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = json{{"type", "number"}, {"maximum", json(value)}};
    return j;
}

TEST(Schema, LessThanOrEqualTo) {

    auto have = LessThanOrEqualTo("test_feature", 1234);
    auto want = lessthanorequalto_json_builder("test_feature", 1234);
    ASSERT_EQ(have->Json(), want);

    have = LessThanOrEqualTo("test_feature", 1234, "type_selector");
    want = lessthanorequalto_json_builder("test_feature", 1234, "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = LessThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    want = lessthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = LessThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = lessthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = LessThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = lessthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = LessThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = lessthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


template <typename T>
static json greaterthan_json_builder(const std::string& feature, const T& value,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = json{{"type", "number"}, {"exclusiveMinimum", json(value)}};
    return j;
}

TEST(Schema, GreaterThan) {

    auto have = GreaterThan("test_feature", 1234);
    auto want = greaterthan_json_builder("test_feature", 1234);
    ASSERT_EQ(have->Json(), want);

    have = GreaterThan("test_feature", 1234, "type_selector");
    want = greaterthan_json_builder("test_feature", 1234, "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = GreaterThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    want = greaterthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = GreaterThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = greaterthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = GreaterThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = greaterthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = GreaterThan("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = greaterthan_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


template <typename T>
static json greaterthanorequalto_json_builder(const std::string& feature, const T& value,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = json{{"type", "number"}, {"minumum", json(value)}};
    return j;
}

TEST(Schema, GreaterThanOrEqualTo) {

    auto have = GreaterThanOrEqualTo("test_feature", 1234);
    auto want = greaterthanorequalto_json_builder("test_feature", 1234);
    ASSERT_EQ(have->Json(), want);

    have = GreaterThanOrEqualTo("test_feature", 1234, "type_selector");
    want = greaterthanorequalto_json_builder("test_feature", 1234, "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = GreaterThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    want = greaterthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = GreaterThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = greaterthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = GreaterThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = greaterthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = GreaterThanOrEqualTo("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = greaterthanorequalto_json_builder("test_feature", 1234, "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


static json isset_json_builder(const std::string& feature,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = {{"not", {{"type", "null"}}}};
    return j;
}

TEST(Schema, IsSet) {

    auto have = IsSet("test_feature");
    auto want = isset_json_builder("test_feature");
    ASSERT_EQ(have->Json(), want);

    have = IsSet("test_feature", "type_selector");
    want = isset_json_builder("test_feature", "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = IsSet("test_feature", "type_selector", schema::ValueEncoding::ENCODED);
    want = isset_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = IsSet("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = isset_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = IsSet("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = isset_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = IsSet("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = isset_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


static json regexmatch_json_builder(const std::string& feature, const std::string& value,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    auto j = constraint_json_builder(feature, schema::ConstraintType::SCHEMA, nullptr, type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
    j["value"] = json{{"type", "string"}, {"pattern", value}};
    return j;
}

TEST(Schema, RegexMatch) {

    auto have = RegexMatch("test_feature", "^[Vv]alid [Ee]xpression$");
    auto want = regexmatch_json_builder("test_feature", "^[Vv]alid [Ee]xpression$");
    ASSERT_EQ(have->Json(), want);

    have = RegexMatch("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector");
    want = regexmatch_json_builder("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = RegexMatch("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED);
    want = regexmatch_json_builder("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = RegexMatch("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = regexmatch_json_builder("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = RegexMatch("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = regexmatch_json_builder("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = RegexMatch("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = regexmatch_json_builder("test_feature", "^[Vv]alid [Ee]xpression$", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
}


static json change_json_builder(const std::string& feature,
           const std::string& type_selector = schema::DEFAULT_TYPE,
           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
           const std::string& path = schema::Constraint::PATH_IDENTITY,
           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true) {
    return constraint_json_builder(feature, schema::ConstraintType::CHANGE, std::shared_ptr<schema::NullType>(), type_selector, value_encoding,
                                     path, instance_filter, limit_feature_selection);
}

TEST(Schema, Change) {

    auto have = Change("test_feature");
    auto want = change_json_builder("test_feature");
    ASSERT_EQ(have->Json(), want);

    have = Change("test_feature", "type_selector");
    want = change_json_builder("test_feature", "type_selector");
    ASSERT_EQ(have->Json(), want);

    have = Change("test_feature", "type_selector", schema::ValueEncoding::ENCODED);
    want = change_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED);
    ASSERT_EQ(have->Json(), want);

    have = Change("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    want = change_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path");
    ASSERT_EQ(have->Json(), want);

    have = Change("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    want = change_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter");
    ASSERT_EQ(have->Json(), want);

    have = Change("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    want = change_json_builder("test_feature", "type_selector", schema::ValueEncoding::ENCODED, "test_path", "test_instance_filter", false);
    ASSERT_EQ(have->Json(), want);
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
            return nullptr;
    }
}

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
    auto j = constraint_json_builder(feature, schema::ConstraintType::NELSON, nullptr, type_selector,
                                     schema::ValueEncoding::ENCODED, "", instance_filter, limit_feature_selection);
    j["value"] = value;

    return j;
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

TEST(Schema, Rule) {
    auto have = schema::Rule(nullptr);
    auto want = json(nullptr);

    ASSERT_EQ(have.Json(), want);

    auto c = std::make_shared<schema::ChangeConstraint>("test_feature");
    have = schema::Rule(c);
    ASSERT_EQ(have.Json(), c->Json());
}

TEST(Schema, AndRules) {
    struct {
        std::vector<schema::rule_ptr> ctor_rules;
        std::vector<schema::rule_ptr> add_rules;
        std::vector<std::string> exclude_on;

        json want;
    } tests[] {
        {
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("ctor_feature1"))
            },
            { },
            { },
            json::parse(R"({
                "rules": [
                    {
                        "feature": "ctor_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null
                    }
                ],
                "type": "and",
                "excludeOn": null
            })")
        },
        {
            { },
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("add_feature1"))
            },
            { },
            json::parse(R"({
                "rules": [
                    {
                        "feature": "add_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    }
                ],
                "type": "and",
                "excludeOn": null
            })")
        },
        {
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("ctor_feature1")),
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("ctor_feature2"))
            },
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("add_feature1")),
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("add_feature2"))
            },
            { "exclude-1", "exclude-2", "exclude-3" },
            json::parse(R"({
                "rules": [
                    {
                        "feature": "ctor_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    },
                    {
                        "feature": "ctor_feature2",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    },
                    {
                        "feature": "add_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    },
                    {
                        "feature": "add_feature2",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    }
                ],
                "type": "and",
                "excludeOn": ["exclude-1", "exclude-2", "exclude-3"]
            })")
        }
    };

    for (const auto& t : tests) {
        auto have = schema::AndRules(t.ctor_rules);

        for (const auto& r : t.add_rules) {
            have.Add(r);
        }
        for (const auto& ex : t.exclude_on) {
            have.ExcludeOn(ex);
        }

        ASSERT_EQ(have.Json(), t.want);
    }
}

TEST(Schema, OrRules) {
    struct {
        std::vector<schema::rule_ptr> ctor_rules;
        std::vector<schema::rule_ptr> add_rules;
        std::vector<std::string> exclude_on;

        json want;
    } tests[] {
        {
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("ctor_feature1"))
            },
            { },
            { },
            json::parse(R"({
                "rules": [
                    {
                        "feature": "ctor_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    }
                ],
                "type": "or",
                "excludeOn": null
            })")
        },
        {
            { },
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("add_feature1"))
            },
            { },
            json::parse(R"({
                "rules": [
                    {
                        "feature": "add_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    }
                ],
                "type": "or",
                "excludeOn": null
            })")
        },
        {
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("ctor_feature1")),
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("ctor_feature2"))
            },
            {
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("add_feature1")),
                std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>("add_feature2"))
            },
            { "exclude-1", "exclude-2", "exclude-3" },
            json::parse(R"({
                "rules": [
                    {
                        "feature": "ctor_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    },
                    {
                        "feature": "ctor_feature2",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    },
                    {
                        "feature": "add_feature1",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    },
                    {
                        "feature": "add_feature2",
                        "instanceIdFilter": ".*",
                        "limitFeatureSelection": true,
                        "op": 1,
                        "path": "",
                        "typeSelector": "default",
                        "valueType": 1,
                        "value": null

                    }
                ],
                "type": "or",
                "excludeOn": ["exclude-1", "exclude-2", "exclude-3"]
            })")
        }
    };

    for (const auto& t : tests) {
        auto have = schema::OrRules(t.ctor_rules);

        for (const auto& r : t.add_rules) {
            have.Add(r);
        }
        for (const auto& ex : t.exclude_on) {
            have.ExcludeOn(ex);
        }

        ASSERT_EQ(have.Json(), t.want);
    }
}

TEST(Schema, OutputEncoding) {
    struct {
        schema::OutputEncoding::Type type;
        json want;
    } tests[] {
        {
            schema::OutputEncoding::Type::Number,
            json::parse(R"({"encoder":null,"type":"number"})")
        },
        {
            schema::OutputEncoding::Type::Boolean,
            json::parse(R"({"encoder":null,"type":"boolean"})")
        },
        {
            schema::OutputEncoding::Type::String,
            json::parse(R"({"encoder":null,"type":"string"})")
        },
        {
            schema::OutputEncoding::Type::Object,
            json::parse(R"({"encoder":null,"type":"object"})")
        },
        {
            schema::OutputEncoding::Type::Any,
            json::parse(R"({"encoder":null,"type":"any"})")
        },
    };


    for (const auto& t : tests) {
        auto have = schema::OutputEncoding{t.type};

        ASSERT_EQ(have.Json(), t.want);
    }
}

TEST(Schema, Metadata) {
    auto m1 = schema::Metadata{"metadata1"};
    auto want1 = json::parse(R"({"description":"metadata1","history":0,"ttl":0,"encoding":{"encoder":null,"type":"object"},"unit":"ONE"})");

    auto m2 = schema::Metadata{"metadata2", 10};
    auto want2 = json::parse(R"({"description":"metadata2","history":10,"ttl":0,"encoding":{"encoder":null,"type":"object"},"unit":"ONE"})");

    auto m3 = schema::Metadata{"metadata3", 10, 30};
    auto want3 = json::parse(R"({"description":"metadata3","history":10,"ttl":30,"encoding":{"encoder":null,"type":"object"},"unit":"ONE"})");

    auto m4 = schema::Metadata{"metadata4", 10, 30, "kilogram"};
    auto want4 = json::parse(R"({"description":"metadata4","history":10,"ttl":30,"encoding":{"encoder":null,"type":"object"},"unit":"kilogram"})");

    auto m5 = schema::Metadata{"metadata5", 10, 30, "kilogram", schema::OutputEncoding{schema::OutputEncoding::Type::Number}};
    auto want5 = json::parse(R"({"description":"metadata5","history":10,"ttl":30,"encoding":{"encoder":null,"type":"number"},"unit":"kilogram"})");

    ASSERT_EQ(m1.Json(), want1);
    ASSERT_EQ(m2.Json(), want2);
    ASSERT_EQ(m3.Json(), want3);
    ASSERT_EQ(m4.Json(), want4);
    ASSERT_EQ(m5.Json(), want5);
}

TEST(Schema, OutputFeature) {
    auto have = schema::OutputFeature{"output_feature", schema::Metadata{"metadata"}};
    auto want = json::parse(R"({"description":"metadata","history":0,"ttl":0,"encoding":{"encoder":null,"type":"object"},"unit":"ONE"})");

    ASSERT_EQ(have.Json(), want);
}

TEST(Schema, SkipCycleCheckType) {
    auto scc_empty = schema::SkipCycleCheckType();
    ASSERT_EQ(scc_empty.Json(), json::parse("[]"));

    auto scc_skip_all = schema::SkipCycleCheckType();
    scc_skip_all.SkipAll();
    ASSERT_EQ(scc_skip_all.Json(), json(true));

    auto scc_skip_some = schema::SkipCycleCheckType();
    scc_skip_some.Skip("alpha");
    ASSERT_EQ(scc_skip_some.Json(), json::parse(R"(["alpha"])"));
    scc_skip_some.Skip("beta");
    ASSERT_EQ(scc_skip_some.Json(), json::parse(R"(["alpha", "beta"])"));
    scc_skip_some.Skip("gamma");
    ASSERT_EQ(scc_skip_some.Json(), json::parse(R"(["alpha", "beta", "gamma"])"));
}

