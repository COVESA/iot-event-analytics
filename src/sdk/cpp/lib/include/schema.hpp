/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#ifndef SCHEMA_HPP
#define SCHEMA_HPP

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

enum class OpType { ISSET, EQUALS, NEQUALS, LESS_THAN, LESS_THAN_EQUAL, GREATER_THAN, GREATER_THAN_EQUAL, REGEX };

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
    Opt() noexcept;
    explicit Opt(const T& value) noexcept;
    explicit operator bool() const noexcept;
    T Get() const;
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
        : OpConstraint{feature, value, type_selector, value_encoding, path, instance_filter, limit_feature_selection}
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

    explicit Rules(const std::string& type);

   public:
    void Add(rule_ptr rule);
    void Add(const rule_vec& rules);
    void Add(const rules_ptr rules);
    json Json() const override;
};

class AndRules : public Rules {
   public:
    explicit AndRules(const rule_vec& rules);
    explicit AndRules(rules_ptr rules);
    explicit AndRules(std::initializer_list<rule_ptr> rules);
};

class OrRules : public Rules {
   public:
    explicit OrRules(const rule_vec& rules);
    explicit OrRules(rules_ptr rules);
    explicit OrRules(std::initializer_list<rule_ptr> rules);
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
    const std::string unit_;
    const OutputEncoding encoding_;

   public:
    explicit Metadata(const std::string& description, const std::string& unit = "ONE",
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
    Opt<bool> skip_ = defbool;
    Opt<std::vector<std::string>> names_ = Opt<std::vector<std::string>>();

   public:
    SkipCycleCheckType();
    explicit SkipCycleCheckType(bool skip);
    explicit SkipCycleCheckType(const std::vector<std::string>& names);
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

class IOFeatures : public SchemaEntity {
   protected:
    std::vector<OutputFeature> output_features_;
    Options options_;
    SkipCycleCheckType scc_;

   public:
    IOFeatures();
    void SkipCycleCheck(const bool skip = true);
    void SkipCycleCheckFor(std::initializer_list<std::string> args);
    void AddOutput(const std::string& feature, const Metadata& metadata);
    const Options& GetOptions() const;
    json GetOutputFeatures(const std::string& talent_id) const;
};

class Schema {
   private:
    const std::string id_;
    const bool remote_;
    const Options options_;
    std::vector<OutputFeature> outputs_;
    rule_ptr rules_;

   public:
    Schema(const std::string& id, const bool remote, const Options& options, const std::vector<OutputFeature>& outputs,
           rule_ptr rules);
    json Json() const;
};

class Talent : public IOFeatures {
   private:
    static std::string CreateUuid(const std::string& prefix);
    rules_ptr rules_;

   protected:
    const std::string id_;
    const std::string uid_;
    std::set<std::string> callees_;

   public:
    explicit Talent(const std::string& id);
    bool IsRemote() const;
    std::string GetFullFeature(const std::string& talent_id, const std::string& feature,
                               const std::string& type = "") const;
    Schema GetSchema(rules_ptr rules) const;
    json Json() const override;
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

schema::rules_ptr OrRules(std::initializer_list<schema::rule_ptr> rules);
schema::rules_ptr AndRules(std::initializer_list<schema::rule_ptr> rules);

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

}  // namespace core
}  // namespace iotea

#endif
