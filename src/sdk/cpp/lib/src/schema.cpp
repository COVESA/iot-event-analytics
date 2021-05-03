/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "schema.hpp"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "util.hpp"

namespace iotea {
namespace core {
namespace schema {

const char Constraint::PATH_IDENTITY[] = "";
const char Constraint::ANY_FEATURE[] = "";
const char Constraint::ALL_TYPES[] = "*";
const char Constraint::SEGMENTS[] = "*";
const char Constraint::ALL_INSTANCE_FILTERS[] = ".*";

//
// ValueType
//
ValueType::ValueType(const std::string& type)
    : type_{type} {}

json ValueType::Json() const { return json{{"type", type_}}; }

//
// NullType
//
NullType::NullType()
    : ValueType{"null"} {}

//
// BooleanType
//
BooleanType::BooleanType()
    : ValueType{"boolean"} {}

//
// NumberType
//
NumberType::NumberType()
    : ValueType{"number"} {}

//
// IntegerType
//
IntegerType::IntegerType()
    : ValueType{"integer"} {}

//
// StringType
//
StringType::StringType(const std::string& const_value)
    : ValueType{"string"}
    , const_value_{const_value} {}

StringType::StringType(const Opt<unsigned int>& min_length, const Opt<unsigned int>& max_length,
                       const Opt<std::string>& pattern)
    : ValueType{"string"}
    , min_length_{min_length}
    , max_length_{max_length}
    , pattern_{pattern} {
    if (min_length && max_length) {
        assert(min_length.Get() <= max_length.Get());
    }

    if (pattern) {
        // TODO verify that pattern is valid regex
    }
}

StringType::StringType(const std::vector<std::string>& enumeration)
    : ValueType("string")
    , enumeration_{enumeration} {}

json StringType::Json() const {
    auto j = ValueType::Json();

    if (const_value_) {
        j["const"] = const_value_.Get();
        return j;
    }

    if (enumeration_) {
        j["enum"] = enumeration_.Get();
        return j;
    }

    if (min_length_) {
        j["min_length"] = min_length_.Get();
    }

    if (max_length_) {
        j["max_length"] = max_length_.Get();
    }

    if (pattern_) {
        j["pattern"] = pattern_.Get();
    }

    return j;
}

//
// ArrayType
//
ArrayType::ArrayType()
    : ValueType("array") {}

ArrayType::ArrayType(const value_vec& items, Opt<unsigned int> min_items, Opt<unsigned int> max_items,
                     Opt<bool> unique_items, Opt<bool> additional_items_)
    : ValueType{"array"}
    , items_{items}
    , min_items_{min_items}
    , max_items_{max_items}
    , unique_items_{unique_items}
    , additional_items_{additional_items_} {
    if (min_items_ && max_items_) {
        assert(min_items_.Get() <= max_items_.Get());
    }
}

ArrayType::ArrayType(value_ptr contains, Opt<unsigned int> min_items, Opt<unsigned int> max_items,
                     Opt<bool> unique_items, Opt<bool> additional_items_)
    : ValueType{"array"}
    , contains_{contains}
    , min_items_{min_items}
    , max_items_{max_items}
    , unique_items_{unique_items}
    , additional_items_{additional_items_} {
    if (min_items_ && max_items_) {
        assert(min_items_.Get() <= max_items_.Get());
    }
}

json ArrayType::Json() const {
    auto j = ValueType::Json();

    if (contains_) {
        j["contains"] = contains_.Get()->Json();
    }

    if (items_) {
        auto items = json::array();

        for (auto& item : items_.Get()) {
            items.push_back(item->Json());
        }

        j["items"] = items;
    }

    if (max_items_) {
        j["max_items"] = max_items_.Get();
    }

    if (min_items_) {
        j["min_items"] = min_items_.Get();
    }

    if (unique_items_) {
        j["unique_items"] = unique_items_.Get();
    }

    if (additional_items_) {
        j["additional_items"] = additional_items_.Get();
    }

    return j;
}

//
// Property
//
Property::Property(const std::string& name, value_ptr value)
    : name_{name}
    , value_{value} {}

json Property::Json() const { return json{{name_, value_->Json()}}; }

//
// Properties
//
Properties::Properties(std::initializer_list<std::pair<const std::string, Property>> properties)
    : value_{properties} {}

json Properties::Json() const {
    json j;

    for (auto& p : value_) {
        j[p.first] = p.second.Json();
    }

    return j;
}

//
// ObjectType
//
ObjectType::ObjectType(const property_map& properties, const std::vector<std::string>& required,
                       bool additional_properties)
    : ValueType("object")
    , properties_{properties}
    , required_{required}
    , additional_properties_{additional_properties} {}

json ObjectType::Json() const {
    auto j = ValueType::Json();

    json props;
    for (auto& p : properties_) {
        props[p.first] = p.second->Json();
    }
    j["properties"] = props;

    if (!required_.empty()) {
        j["required"] = required_;
    }

    j["additionalProperties"] = additional_properties_;

    return j;
}

//
// Constraint
//
Constraint::Constraint(const std::string& feature, const ConstraintType constraint_type,
                       std::shared_ptr<SchemaEntity> value, const std::string& type_selector,
                       const ValueEncoding value_encoding, const std::string& path, const std::string& instance_filter,
                       const bool limit_feature_selection)
    : feature_{feature}
    , constraint_type_{constraint_type}
    , value_{value}
    , type_selector_{type_selector}
    , value_encoding_{value_encoding}
    , path_{path}
    , instance_filter_{instance_filter}
    , limit_feature_selection_(limit_feature_selection) {
    if (!limit_feature_selection_ && feature_ != ANY_FEATURE) {
        // TODO warn that:
        // Not setting limit_feature_selection only has effect if feature is ANY_FEATURE
    }

    static const auto expr = std::regex{R"((?:^(\*|(?:[^\.]+))\.)?([^\.]+)$)"};
    std::cmatch m;

    if (!std::regex_match(type_selector_.c_str(), m, expr)) {
        throw std::invalid_argument("Invalid constraint");
    }

    type_ = m[1];
    segment_ = m[2];
}

json Constraint::Json() const {
    auto j = json{
        {"feature", feature_},
        {"op", constraint_type_},
        {"typeSelector", type_selector_},
        {"valueType", value_encoding_},
        {"path", path_},
        {"instanceIdFilter", instance_filter_},
        {"limitFeatureSelection", limit_feature_selection_},
    };

    j["value"] = value_ ? value_->Json() : json(nullptr);

    return j;
}

//
// SchemaConstraint
//
SchemaConstraint::SchemaConstraint(const std::string& feature, std::shared_ptr<SchemaEntity> value,
                                   const std::string& type_selector, const ValueEncoding value_encoding,
                                   const std::string& path, const std::string& instance_filter,
                                   const bool limit_feature_selection, const std::string& uuid)
    : Constraint(feature, ConstraintType::SCHEMA, value, type_selector, value_encoding, path, instance_filter,
                 limit_feature_selection)
    , uuid_{uuid} {}

//
// IsSet
//
IsSet::IsSet(const std::string& feature, const std::string& type_selector, const ValueEncoding value_encoding,
             const std::string& path, const std::string& instance_filter, const bool limit_feature_selection)
    : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection} {}

json IsSet::Json() const {
    auto j = OpConstraint::Json();
    j["value"] = {{"not", {{"type", "null"}}}};

    return j;
}

//
// RegexMatch
//
RegexMatch::RegexMatch(const std::string& feature, const std::string& value, const std::string& type_selector,
                       const ValueEncoding value_encoding, const std::string& path, const std::string& instance_filter,
                       const bool limit_feature_selection)
    : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
    , value_{value} {}

json RegexMatch::Json() const {
    auto j = OpConstraint::Json();
    j["value"] = json{{"type", "string"}, {"pattern", value_}};

    return j;
}

//
// OpConstraint
//
OpConstraint::OpConstraint(const std::string& feature, std::shared_ptr<SchemaEntity> value,
                           const std::string& type_selector, const ValueEncoding value_encoding,
                           const std::string& path, const std::string& instance_filter,
                           const bool limit_feature_selection)
    : Constraint(feature, ConstraintType::SCHEMA, value, type_selector, value_encoding, path, instance_filter,
                 limit_feature_selection) {}

//
// ChangeConstraint
//
ChangeConstraint::ChangeConstraint(const std::string& feature, const std::string& type_selector,
                                   const ValueEncoding value_encoding, const std::string& path,
                                   const std::string& instance_filter, const bool limit_feature_selection)
    : Constraint{feature, ConstraintType::CHANGE, std::shared_ptr<NullType>(), type_selector, value_encoding, path,
                 instance_filter, limit_feature_selection} {}

//
// TimeseriesConstraint
//
TimeseriesConstraint::TimeseriesConstraint(const std::string& feature, const ConstraintType constraint_type,
                                           std::shared_ptr<SchemaEntity> value, const std::string& type_selector,
                                           const ValueEncoding value_encoding, const std::string& path,
                                           const std::string& instance_filter, const bool limit_feature_selection)
    : Constraint{feature, constraint_type, value, type_selector, value_encoding, path, instance_filter,
                 limit_feature_selection} {}

//
// NelsonConstraint
//
NelsonConstraint::NelsonConstraint(const std::string& feature, NelsonConstraint::Type value,
                                   const std::string& type_selector,
                                   const std::string& instance_filter,
                                   const bool limit_feature_selection)
    : TimeseriesConstraint{feature, ConstraintType::NELSON, nullptr, type_selector, ValueEncoding::ENCODED, "",
                           instance_filter, limit_feature_selection}
    , value_{value} {}

json NelsonConstraint::Json() const {
    auto j = TimeseriesConstraint::Json();
    j["value"] = value_;

    return j;
}

//
// Rule
//
Rule::Rule(std::shared_ptr<Constraint> constraint)
    : constraint_{constraint} {}

json Rule::Json() const {
    return constraint_.get() == nullptr ? json(nullptr) : constraint_->Json();
}

//
// Rules
//
Rules::Rules(const std::string& type)
    : type_{type} {}

void Rules::Add(rule_ptr rule) { rules_.push_back(rule); }

void Rules::ExcludeOn(const std::string& feature) { exclude_on_.push_back(feature); }

void Rules::ExcludeOn(const std::vector<std::string>& features) {
    exclude_on_.insert(exclude_on_.end(), features.begin(), features.end());
}

json Rules::Json() const {
    auto array = json::array();

    std::transform(rules_.begin(), rules_.end(), std::back_inserter(array), [](rule_ptr r) { return r->Json(); });

    return json{
        {"type", json(type_)},
        {"excludeOn", exclude_on_.empty() ? json(nullptr) : json(exclude_on_)},
        {"rules", array},
    };
}

//
// OutputEncoder
//
json OutputEncoder::Json() const { return nullptr; }

//
// OutputEncoding
//
OutputEncoding::OutputEncoding(const OutputEncoding::Type type, const OutputEncoder& encoder)
    : type_{type}
    , encoder_{encoder} {}

json OutputEncoding::Json() const {
    json type;
    switch (type_) {
        case Type::Number:
            type = json("number");
            break;
        case Type::Boolean:
            type = json("boolean");
            break;
        case Type::String:
            type = json("string");
            break;
        case Type::Object:
            type = json("object");
            break;
        case Type::Any:
            type = json("any");
            break;
    }

    return json{{"type", type}, {"encoder", encoder_.Json()}};
}

//
// Metadata
//
Metadata::Metadata(const std::string& description, int history, int ttl, const std::string& unit, const OutputEncoding& encoding)
    : description_{description}
    , history_{history}
    , ttl_{ttl}
    , unit_{unit}
    , encoding_{encoding} {}

json Metadata::Json() const {
    return json{
        {"description", description_},
        {"history", history_},
        {"ttl", ttl_},
        {"encoding", encoding_.Json()},
        {"unit", unit_}};
}

//
// OutputFeature
//
OutputFeature::OutputFeature(const std::string& feature, const Metadata& metadata)
    : feature_{feature}
    , metadata_{metadata} {}

const std::string& OutputFeature::GetFeature() { return feature_; }

json OutputFeature::Json() const { return metadata_.Json(); }

//
// SkipCycleCheckType
//
SkipCycleCheckType::SkipCycleCheckType()
    : skip_{false} {}


void SkipCycleCheckType::Skip(const std::string& name) {
    if (std::find(names_.begin(), names_.end(), name) == names_.end()) {
        names_.push_back(name);
    }
}

void SkipCycleCheckType::SkipAll() {
    skip_ = true;
}

json SkipCycleCheckType::Json() const {
    if (skip_) {
        return skip_;
    }

    return names_;
}

//
// Options
//
Options::Options() {}

Options::Options(const options_map& options)
    : options_{options} {}

entity_ptr& Options::operator[](const std::string& key) { return options_[key]; }

json Options::Json() const {
    json options;

    for (auto pair : options_) {
        options[pair.first] = pair.second->Json();
    }

    return options;
}

//
// FunctionValue
//
FunctionValue::FunctionValue(const std::string& name)
    : ObjectType(property_map({{"func", std::make_shared<StringType>(name)},
                               {"args", std::make_shared<ArrayType>()},
                               {"chnl", std::make_shared<StringType>()},
                               {"call", std::make_shared<StringType>()},
                               {"timeoutAtMs", std::make_shared<IntegerType>()}}),
                 std::vector<std::string>({"func", "args", "chnl", "call", "timeoutAtMs"})) {}

//
// Schema
//
Schema::Schema(const std::string& id, const std::vector<OutputFeature>& outputs, const options_map& options, rule_ptr rules)
    : id_{id}
    , outputs_{outputs}
    , options_{options}
    , rules_{rules} {}

json Schema::Json() const {
    auto features = json::object();

    for (auto o : outputs_) {
        features[id_ + "." + o.GetFeature()] = o.Json();
    }

    auto config = json{
        {"outputs", features},
        {"rules", rules_->Json()}
    };

    for (const auto& opt : options_) {
        config[opt.first] = opt.second->Json();
    }

    return json{
        {"id", id_},
        {"config", config}
    };
}

//
// Talent
//
Talent::Talent(const std::string& id)
    : id_{id}
    , scc_{std::make_shared<SkipCycleCheckType>()}
    , options_{{"scc", scc_}} {}

Schema Talent::GetSchema(rule_ptr rules) const { return Schema(id_, output_features_, options_, rules); }

void Talent::SkipCycleChecks() {
    scc_->SkipAll();
}

void Talent::SkipCycleCheckFor(const std::string& feature) {
    scc_->Skip(feature);
}

void Talent::AddOutput(const std::string& feature, const Metadata& metadata) {
    // TODO handle situation where the same feature is added twice
    output_features_.push_back(OutputFeature(feature, metadata));
}

//
// Feature
//
Feature::Feature(const std::string& description)
    : ObjectType(property_map({{"description", std::make_shared<StringType>(description)}}),
                 std::vector<std::string>{"description", "required"}) {}

//
// Encoding
//
Encoding::Encoding(const property_map& properties, const std::vector<std::string>& required)
    : ObjectType(properties, required) {}

//
// NullEncoding
//
NullEncoding::NullEncoding()
    : Encoding(property_map({{"encoder", std::make_shared<StringType>("null")},
                             {"type", std::make_shared<StringType>(
                                          std::vector<std::string>{"number", "boolean", "string", "object", "any"})}}),
               std::vector<std::string>({"type"})) {}

//
// ThroughEncoding
//
ThroughEncoding::ThroughEncoding()
    : Encoding(property_map({{"encoder", std::make_shared<StringType>("through")},
                             {"type", std::make_shared<StringType>(std::vector<std::string>{"number"})},
                             {"reduce", std::make_shared<StringType>()}}),
               std::vector<std::string>({"type", "encoder"})) {}

//
// MinmaxEncoding
//
MinmaxEncoding::MinmaxEncoding()
    : Encoding(property_map({{"encoder", std::make_shared<StringType>("minmax")},
                             {"type", std::make_shared<StringType>(std::vector<std::string>{"number", "object"})},
                             {"min", std::make_shared<NumberType>()},
                             {"max", std::make_shared<NumberType>()},
                             {"reduce", std::make_shared<StringType>()}}),
               std::vector<std::string>({"type", "encoder", "min", "max"})) {}

//
// DeltaEncoding
//
DeltaEncoding::DeltaEncoding()
    : Encoding(property_map({{"encoder", std::make_shared<StringType>("delta")},
                             {"type", std::make_shared<StringType>(std::vector<std::string>{"number", "object"})},
                             {"reduce", std::make_shared<StringType>()}}),
               std::vector<std::string>({"type", "encoder"})) {}

//
// CategoryEncoding
//
CategoryEncoding::CategoryEncoding()
    : Encoding(property_map({{"encoder", std::make_shared<StringType>("category")},
                             {"type", std::make_shared<StringType>(
                                          std::vector<std::string>{"number", "boolean", "string", "object"})},
                             {"reduce", std::make_shared<StringType>()},
                             {"enum", std::make_shared<ArrayType>()}}),
               std::vector<std::string>({"type", "encoder", "enum"})) {}

//
// Event
//
Event::Event(const MsgType msg_type, const std::string& subject, const std::string& segment, const std::string& feature,
             const std::string& instance, value_ptr value, const std::string& return_topic)
    : msg_type_{msg_type}
    , subject_{subject}
    , segment_{segment}
    , feature_{feature}
    , instance_{instance}
    , value_{value}
    , return_topic_{return_topic} {}

MsgType Event::GetMsgType() { return msg_type_; }

std::string Event::GetSubject() const { return subject_; }

std::string Event::GetSegment() const { return segment_; }

std::string Event::GetFeature() const { return feature_; }

std::string Event::GetInstance() const { return instance_; }

value_ptr Event::GetValue() const { return value_; }

std::string Event::GetReturnTopic() const { return return_topic_; }

}  // namespace schema

schema::rule_ptr IsSet(const std::string& feature, const std::string& type_selector,
                       const schema::ValueEncoding value_encoding, const std::string& path,
                       const std::string& instance_filter, const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::IsSet>(feature, type_selector, value_encoding, path,
                                                                          instance_filter, limit_feature_selection));
}

schema::rule_ptr RegexMatch(const std::string& feature, const std::string& value, const std::string& type_selector,
                            const schema::ValueEncoding value_encoding, const std::string& path,
                            const std::string& instance_filter, const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::RegexMatch>(
        feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection));
}

schema::rule_ptr Change(const std::string& feature, const std::string& type_selector,
                        const schema::ValueEncoding value_encoding, const std::string& path,
                        const std::string& instance_filter, const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::ChangeConstraint>(feature, type_selector, value_encoding, path,
                                                                          instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonAlter(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::ALTER, type_selector, instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonTrend(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::TREND, type_selector, instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonBias(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::BIAS, type_selector, instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonHighDev(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::HIGH_DEV, type_selector, instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonLowDev(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::LOW_DEV, type_selector, instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonOut1Se(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::OUT1_SE, type_selector, instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonOut2Se(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::OUT2_SE, type_selector, instance_filter, limit_feature_selection));
}

schema::rule_ptr NelsonOut3Se(const std::string& feature,
                                const std::string& type_selector,
                                const std::string& instance_filter,
                                const bool limit_feature_selection) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NelsonConstraint>(
        feature, schema::NelsonConstraint::Type::OUT3_SE, type_selector, instance_filter, limit_feature_selection));
}

}  // namespace core
}  // namespace iotea
