/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef IOTEA_SCHEMA_HPP
#define IOTEA_SCHEMA_HPP

#include <memory>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace iotea {
namespace core {
namespace schema {

// Forward declare types so that we can create type aliases at the top
class SchemaEntity;
class ValueType;
class Rule;
class Rules;

// Type aliases
using entity_ptr = std::shared_ptr<SchemaEntity>;
using value_ptr = std::shared_ptr<ValueType>;
using rule_ptr = std::shared_ptr<Rule>;
using rules_ptr = std::shared_ptr<Rules>;

using property_map = std::unordered_map<std::string, value_ptr>;
using options_map = std::unordered_map<std::string, entity_ptr>;
using value_vec = std::vector<value_ptr>;
using rule_vec = std::vector<rule_ptr>;

const char DEFAULT_TYPE[] = "default";

enum class ConstraintType { SCHEMA = 0, CHANGE = 1, NELSON = 2 };

enum class ValueEncoding { RAW, ENCODED };

enum class MsgType {
    MSG_OK = 1,
    MSG_DISCOVER = 2,
    MSG_UNKNOWN_ATM = 3,  // TODO what this means and if it is even defined is unknown at the moment
    MSG_ERR = 4
};

template <typename T>
class Opt {
   private:
    bool is_set_;
    T value_;

   public:
    Opt() noexcept
        : is_set_(false)
        , value_{T()} {}

    explicit Opt(const T& value) noexcept
        : is_set_{true}
        , value_{value} {}

    operator bool() const noexcept { return is_set_; }

    T Get() const { return value_; }
};

#define defbool Opt<bool>()
#define defint Opt<int>()
#define defuint Opt<unsigned int>()
#define defdouble Opt<double>()
#define defstr Opt<std::string>()

class SchemaEntity {
   public:
    virtual ~SchemaEntity() {}
    virtual json Json() const = 0;
};

class ValueType : public SchemaEntity {
   private:
    const std::string type_;

   protected:
    explicit ValueType(const std::string& type);

   public:
    json Json() const override;
};

class NullType : public ValueType {
   public:
    NullType();
};

class BooleanType : public ValueType {
   public:
    BooleanType();
};

class NumberType : public ValueType {
   public:
    NumberType();
};

class IntegerType : public ValueType {
   public:
    IntegerType();
};

class StringType : public ValueType {
   private:
    const Opt<unsigned int> min_length_ = defuint;
    const Opt<unsigned int> max_length_ = defuint;
    const Opt<std::string> const_value_ = defstr;
    const Opt<std::string> pattern_ = defstr;
    const Opt<std::vector<std::string>> enumeration_ = Opt<std::vector<std::string>>();

   public:
    explicit StringType(const std::string& const_value);
    StringType(const Opt<unsigned int>& min_length = defuint, const Opt<unsigned int>& max_length = defuint,
               const Opt<std::string>& pattern = defstr);
    explicit StringType(const std::vector<std::string>& enumeration);
    json Json() const override;
};

class ArrayType : public ValueType {
   private:
    const Opt<value_vec> items_ = Opt<value_vec>();
    const Opt<value_ptr> contains_ = Opt<value_ptr>();
    const Opt<unsigned int> min_items_ = defuint;
    const Opt<unsigned int> max_items_ = defuint;
    const Opt<bool> unique_items_ = defbool;
    const Opt<bool> additional_items_ = defbool;

   public:
    ArrayType();

    explicit ArrayType(const value_vec& items, Opt<unsigned int> min_items = defuint,
                       Opt<unsigned int> max_items = defuint, Opt<bool> unique_items = defbool,
                       Opt<bool> additional_items_ = defbool);

    explicit ArrayType(value_ptr contains, Opt<unsigned int> min_items = defuint, Opt<unsigned int> max_items = defuint,
                       Opt<bool> unique_items = defbool, Opt<bool> additional_items_ = defbool);

    json Json() const override;
};

class Property : public SchemaEntity {
   private:
    std::string name_;
    value_ptr value_;

   public:
    Property(const std::string& name, value_ptr value);
    json Json() const override;
};

class Properties : public SchemaEntity {
   private:
    const std::unordered_map<std::string, Property> value_;

   public:
    explicit Properties(std::initializer_list<std::pair<const std::string, Property>> properties);
    json Json() const override;
};

class ObjectType : public ValueType {
   private:
    const property_map properties_;
    const std::vector<std::string> required_;
    const bool additional_properties_;

   public:
    explicit ObjectType(const property_map& properties, const std::vector<std::string>& required = {},
                        bool additional_properties = false);
    json Json() const override;
};

class Constraint : public SchemaEntity {
   public:
    static const char PATH_IDENTITY[];
    static const char ANY_FEATURE[];
    static const char ALL_TYPES[];
    static const char SEGMENTS[];
    static const char ALL_INSTANCE_FILTERS[];

    const std::string feature_;
    const ConstraintType constraint_type_;
    std::shared_ptr<SchemaEntity> value_;
    const std::string type_selector_;
    const ValueEncoding value_encoding_;
    const std::string path_;
    const std::string instance_filter_;
    const bool limit_feature_selection_;
    std::string type_;
    std::string segment_;

    Constraint(const std::string& feature, const ConstraintType constraint_type, std::shared_ptr<SchemaEntity> value,
               const std::string& type_selector, const ValueEncoding value_encoding, const std::string& path,
               const std::string& instance_filter, const bool limit_feature_selection);
    json Json() const override;
};

class SchemaConstraint : public Constraint {
   private:
    const std::string uuid_;

   public:
    SchemaConstraint(const std::string& feature, std::shared_ptr<SchemaEntity> value, const std::string& type_selector,
                     const ValueEncoding value_encoding, const std::string& path = Constraint::PATH_IDENTITY,
                     const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                     const bool limit_feature_selection = true, const std::string& uuid = "");
};

class OpConstraint : public Constraint {
   public:
    OpConstraint(const std::string& feature, std::shared_ptr<SchemaEntity> value, const std::string& type_selector,
                 const ValueEncoding value_encoding, const std::string& path = Constraint::PATH_IDENTITY,
                 const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                 const bool limit_feature_selection = true);
};

class IsSet : public OpConstraint {
   public:
    explicit IsSet(const std::string& feature, const std::string& type_selector = DEFAULT_TYPE,
                   const ValueEncoding value_encoding = ValueEncoding::RAW,
                   const std::string& path = Constraint::PATH_IDENTITY,
                   const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                   const bool limit_feature_selection = true);

    json Json() const override;
};

template <typename T>
class Equals : public OpConstraint {
   private:
    const T value_;

   public:
    Equals(const std::string& feature, const T& value,
           const std::string& type_selector = DEFAULT_TYPE, const ValueEncoding value_encoding = ValueEncoding::RAW,
           const std::string& path = Constraint::PATH_IDENTITY,
           const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
           const bool limit_feature_selection = true)
        : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
        , value_{value} {}

    json Json() const override {
        auto j = OpConstraint::Json();
        j["value"] = json{{"const", json(value_)}};
        return j;
    }
};

template <typename T>
class NotEquals : public OpConstraint {
   private:
    const T value_;

   public:
    NotEquals(const std::string& feature, const T& value,
              const std::string& type_selector = DEFAULT_TYPE, const ValueEncoding value_encoding = ValueEncoding::RAW,
              const std::string& path = Constraint::PATH_IDENTITY,
              const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
              const bool limit_feature_selection = true)
        : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
        , value_{value} {}

    json Json() const override {
        auto j = OpConstraint::Json();
        j["value"] = json{{"not", {{"const", json(value_)}}}};

        return j;
    }
};

template <typename T>
class LessThan : public OpConstraint {
   private:
    const T value_;

   public:
    LessThan(const std::string& feature, const T& value, const std::string& type_selector = DEFAULT_TYPE,
             const ValueEncoding value_encoding = ValueEncoding::RAW,
             const std::string& path = Constraint::PATH_IDENTITY,
             const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
             const bool limit_feature_selection = true)
        : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
        , value_{value} {}

    json Json() const override {
        auto j = OpConstraint::Json();
        j["value"] = json{{"type", "number"}, {"exclusiveMaximum", json(value_)}};

        return j;
    }
};

template <typename T>
class LessThanOrEqualTo : public OpConstraint {
   private:
    const T value_;

   public:
    LessThanOrEqualTo(const std::string& feature, const T& value, const std::string& type_selector = DEFAULT_TYPE,
                      const ValueEncoding value_encoding = ValueEncoding::RAW,
                      const std::string& path = Constraint::PATH_IDENTITY,
                      const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                      const bool limit_feature_selection = true)
        : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
        , value_{value} {}

    json Json() const override {
        auto j = OpConstraint::Json();
        j["value"] = json{{"type", "number"}, {"maximum", json(value_)}};

        return j;
    }
};

template <typename T>
class GreaterThan : public OpConstraint {
   private:
    const T value_;

   public:
    GreaterThan(const std::string& feature, const T& value, const std::string& type_selector = DEFAULT_TYPE,
                const ValueEncoding value_encoding = ValueEncoding::RAW,
                const std::string& path = Constraint::PATH_IDENTITY,
                const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                const bool limit_feature_selection = true)
        : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
        , value_{value} {}

    json Json() const override {
        auto j = OpConstraint::Json();
        j["value"] = json{{"type", "number"}, {"exclusiveMinimum", json(value_)}};

        return j;
    }
};

template <typename T>
class GreaterThanOrEqualTo : public OpConstraint {
   private:
    const T value_;

   public:
    GreaterThanOrEqualTo(const std::string& feature, const T& value, const std::string& type_selector = DEFAULT_TYPE,
                         const ValueEncoding value_encoding = ValueEncoding::RAW,
                         const std::string& path = Constraint::PATH_IDENTITY,
                         const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                         const bool limit_feature_selection = true)
        : OpConstraint{feature, nullptr, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
        , value_{value} {}

    json Json() const override {
        auto j = OpConstraint::Json();
        j["value"] = json{{"type", "number"}, {"minumum", json(value_)}};

        return j;
    }
};

class RegexMatch : public OpConstraint {
   private:
    const std::string value_;

   public:
    RegexMatch(const std::string& feature, const std::string& value, const std::string& type_selector = DEFAULT_TYPE,
               const ValueEncoding value_encoding = ValueEncoding::RAW,
               const std::string& path = Constraint::PATH_IDENTITY,
               const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
               const bool limit_feature_selection = true);
    json Json() const override;
};

class ChangeConstraint : public Constraint {
   public:
    explicit ChangeConstraint(const std::string& feature, const std::string& type_selector = DEFAULT_TYPE,
                              const ValueEncoding value_encoding = ValueEncoding::ENCODED,
                              const std::string& path = Constraint::PATH_IDENTITY,
                              const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                              const bool limit_feature_selection = true);
};

class TimeseriesConstraint : public Constraint {
   public:
    TimeseriesConstraint(const std::string& feature, const ConstraintType constraint_type, std::shared_ptr<SchemaEntity> value,
                         const std::string& type_selector = DEFAULT_TYPE,
                         const ValueEncoding value_encoding = ValueEncoding::ENCODED,
                         const std::string& path = Constraint::PATH_IDENTITY,
                         const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                         const bool limit_feature_selection = true);
};

class NelsonConstraint : public TimeseriesConstraint {
   public:
    enum class Type {
        OUT3_SE = 0,
        OUT2_SE = 1,
        OUT1_SE = 2,
        BIAS = 3,
        TREND = 4,
        ALTER = 5,
        LOW_DEV = 6,
        HIGH_DEV = 7
    };

   private:
    const Type value_;

   public:
    NelsonConstraint(const std::string& feature, Type value,
                         const std::string& type_selector = DEFAULT_TYPE,
                         const std::string& instance_filter = Constraint::ALL_INSTANCE_FILTERS,
                         const bool limit_feature_selection = true);

    json Json() const override;
};

class Rule : public SchemaEntity {
   private:
    std::shared_ptr<Constraint> constraint_;

   public:
    explicit Rule(std::shared_ptr<Constraint> constraint = nullptr);
    json Json() const override;
};

class Rules : public Rule {
   private:
    const std::string type_;

   protected:
    rule_vec rules_;
    std::vector<std::string> exclude_on_;

    explicit Rules(const std::string& type);

    template<typename T, typename... Args>
    Rules(const T& type, Args... args)
    : type_{type}
    , rules_{args...} {}

   public:
    void Add(rule_ptr rule);
    void ExcludeOn(const std::string& feature);
    void ExcludeOn(const std::vector<std::string>& features);
    json Json() const override;
};

class AndRules : public Rules {
   public:
    template<typename... Args>
    explicit AndRules(Args... args)
        : Rules{"and", args...} {}
};

class OrRules : public Rules {
   public:
    template<typename... Args>
    explicit OrRules(Args... args)
        : Rules{"or", args...} {}
};

class OutputEncoder : public SchemaEntity {
   public:
    json Json() const override;
};

class OutputEncoding : public SchemaEntity {
   public:
    enum class Type { Number, Boolean, String, Object, Any };

   private:
    const Type type_;
    const OutputEncoder encoder_;

   public:
    explicit OutputEncoding(const OutputEncoding::Type type = Type::Object,
                            const OutputEncoder& encoder = OutputEncoder());
    json Json() const override;
};

class Metadata : public SchemaEntity {
   private:
    const std::string description_;
    const int history_;
    const int ttl_;
    const std::string unit_;
    const OutputEncoding encoding_;

   public:
    explicit Metadata(const std::string& description, int history = 0, int ttl = 0,
            const std::string& unit = "ONE",
            const OutputEncoding& encoding = OutputEncoding());
    json Json() const override;
};

class OutputFeature : public SchemaEntity {
   private:
    const std::string feature_;
    const Metadata metadata_;

   public:
    OutputFeature(const std::string& feature, const Metadata& metadata);
    const std::string& GetFeature();
    json Json() const override;
};

class SkipCycleCheckType : public SchemaEntity {
   private:
    bool skip_;
    std::vector<std::string> names_;

   public:
    SkipCycleCheckType();

    void Skip(const std::string& name);
    void SkipAll();

    json Json() const override;
};

class Options : public SchemaEntity {
   private:
    options_map options_;

   public:
    Options();
    explicit Options(const options_map& options);
    entity_ptr& operator[](const std::string& key);
    json Json() const override;
};

class FunctionValue : public ObjectType {
   public:
    explicit FunctionValue(const std::string& name);
};

class Schema {
   private:
    const std::string id_;
    std::vector<OutputFeature> outputs_;
    options_map options_;
    rule_ptr rules_;

   public:
    Schema(const std::string& id, const std::vector<OutputFeature>& outputs,
           const options_map& options, rule_ptr rules);
    json Json() const;
};

class Talent {
   private:
    const std::string id_;
    std::shared_ptr<SkipCycleCheckType> scc_;
    options_map options_;
    std::vector<OutputFeature> output_features_;
    std::set<std::string> callees_;

   public:
    explicit Talent(const std::string& id);
    Schema GetSchema(rule_ptr rules) const;
    void SkipCycleChecks();
    void SkipCycleCheckFor(const std::string& feature);
    void AddOutput(const std::string& feature, const Metadata& metadata);
};

class Feature : public ObjectType {
   public:
    explicit Feature(const std::string& description);
};

class Encoding : public ObjectType {
   public:
    Encoding(const property_map& properties, const std::vector<std::string>& required);
};

class NullEncoding : public Encoding {
   public:
    NullEncoding();
};

class ThroughEncoding : public Encoding {
   public:
    ThroughEncoding();
};

class MinmaxEncoding : public Encoding {
   public:
    MinmaxEncoding();
};

class DeltaEncoding : public Encoding {
   public:
    DeltaEncoding();
};

class CategoryEncoding : public Encoding {
   public:
    CategoryEncoding();
};

class Event {
   private:
    const MsgType msg_type_;
    const std::string subject_;
    const std::string segment_;
    const std::string feature_;
    const std::string instance_;
    const std::shared_ptr<ValueType> value_;
    const std::string return_topic_;

   public:
    Event(const MsgType msg_type, const std::string& subject, const std::string& segment, const std::string& feature,
          const std::string& instance, value_ptr value, const std::string& return_topic);
    MsgType GetMsgType();
    std::string GetSubject() const;
    std::string GetSegment() const;
    std::string GetFeature() const;
    std::string GetInstance() const;
    value_ptr GetValue() const;
    std::string GetReturnTopic() const;
};

}  // namespace schema

template<typename... Args>
schema::rules_ptr OrRules(Args... args) {
    return std::make_shared<schema::OrRules>(args...);
}

template<typename... Args>
schema::rules_ptr AndRules(Args... args) {
    return std::make_shared<schema::AndRules>(args...);
}

schema::rule_ptr IsSet(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                       const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                       const std::string& path = schema::Constraint::PATH_IDENTITY,
                       const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                       const bool limit_feature_selection = true);

template <typename T>
schema::rule_ptr Equals(const std::string& feature, const T& value,
                        const std::string& type_selector = schema::DEFAULT_TYPE,
                        const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                        const std::string& path = schema::Constraint::PATH_IDENTITY,
                        const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                        const bool limit_feature_selection = true) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::Equals<T>>(
        feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection));
}

template <typename T>
schema::rule_ptr NotEquals(const std::string& feature, const T& value,
                           const std::string& type_selector = schema::DEFAULT_TYPE,
                           const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                           const std::string& path = schema::Constraint::PATH_IDENTITY,
                           const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                           const bool limit_feature_selection = true) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::NotEquals<T>>(
        feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection));
}


template <typename T>
schema::rule_ptr LessThan(const std::string& feature, const T& value,
                          const std::string& type_selector = schema::DEFAULT_TYPE,
                          const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                          const std::string& path = schema::Constraint::PATH_IDENTITY,
                          const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                          const bool limit_feature_selection = true) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::LessThan<T>>(
        feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection));
}

template <typename T>
schema::rule_ptr LessThanOrEqualTo(const std::string& feature, const T& value,
                                   const std::string& type_selector = schema::DEFAULT_TYPE,
                                   const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                                   const std::string& path = schema::Constraint::PATH_IDENTITY,
                                   const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                                   const bool limit_feature_selection = true) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::LessThanOrEqualTo<T>>(
        feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection));
}

template <typename T>
schema::rule_ptr GreaterThan(const std::string& feature, const T& value,
                             const std::string& type_selector = schema::DEFAULT_TYPE,
                             const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                             const std::string& path = schema::Constraint::PATH_IDENTITY,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::GreaterThan<T>>(
        feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection));
}

template <typename T>
schema::rule_ptr GreaterThanOrEqualTo(const std::string& feature, const T& value,
                                      const std::string& type_selector = schema::DEFAULT_TYPE,
                                      const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                                      const std::string& path = schema::Constraint::PATH_IDENTITY,
                                      const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                                      const bool limit_feature_selection = true) {
    return std::make_shared<schema::Rule>(std::make_shared<schema::GreaterThanOrEqualTo<T>>(
        feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection));
}

schema::rule_ptr RegexMatch(const std::string& feature, const std::string& value,
                            const std::string& type_selector = schema::DEFAULT_TYPE,
                            const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                            const std::string& path = schema::Constraint::PATH_IDENTITY,
                            const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                            const bool limit_feature_selection = true);

schema::rule_ptr Change(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                       const schema::ValueEncoding value_encoding = schema::ValueEncoding::RAW,
                       const std::string& path = schema::Constraint::PATH_IDENTITY,
                       const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                       const bool limit_feature_selection = true);

schema::rule_ptr NelsonAlter(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

schema::rule_ptr NelsonTrend(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

schema::rule_ptr NelsonBias(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

schema::rule_ptr NelsonHighDev(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

schema::rule_ptr NelsonLowDev(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

schema::rule_ptr NelsonOut1Se(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

schema::rule_ptr NelsonOut2Se(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

schema::rule_ptr NelsonOut3Se(const std::string& feature, const std::string& type_selector = schema::DEFAULT_TYPE,
                             const std::string& instance_filter = schema::Constraint::ALL_INSTANCE_FILTERS,
                             const bool limit_feature_selection = true);

}  // namespace core
}  // namespace iotea

#endif // IOTEA_SCHEMA_HPP_
