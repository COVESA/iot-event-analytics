/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "event.hpp"
#include "schema.hpp"
#include "talent.hpp"

namespace iotea {
namespace core {

//
// Talent
//
Talent::Talent(const std::string& talent_id)
    : schema_{schema::Talent{talent_id}}
    , talent_id_{talent_id}
    , logger_{std::string{"Talent."} + talent_id} {}

void Talent::Initialize(reply_handler_ptr reply_handler, context_generator_func_ptr context_gen, uuid_generator_func_ptr uuid_gen) {
    reply_handler_ = reply_handler;
    context_gen_ = context_gen;
    uuid_gen_ = uuid_gen;

    channel_id_ = GetId() + "." + uuid_gen_();
}

std::string Talent::GetId() const { return talent_id_; }

Callee Talent::RegisterCallee(const std::string& talent_id, const std::string& func,
       const std::string& type) {
    auto c = Callee{talent_id, func, type};
    callees_.push_back(c);

    auto feature_out = GetOutputName(DEFAULT_TYPE, talent_id_, func);
    schema_.SkipCycleCheckFor(feature_out);
    return c;
}

std::vector<Callee> Talent::GetCallees() {
    return callees_;
}

std::string Talent::GetChannelId() const { return channel_id_; }

void Talent::AddOutput(const std::string& feature, const schema::Metadata& metadata) {
    schema_.AddOutput(feature, metadata);
}

event_ctx_ptr Talent::NewEventContext(const std::string& subject) {
    return context_gen_(subject);
}

schema::rules_ptr Talent::GetRules() const {
    if (callees_.empty()) {
        return nullptr;
    }

    auto rules = schema::rule_vec{};
    auto chan_expr = R"(^\/)" + talent_id_ + R"(\.[^\/]+\/.*)";

    for (const auto& c : callees_) {
        auto output_feature = GetOutputName(c.GetFeature());
        auto r = std::make_shared<schema::Rule>(std::make_shared<schema::RegexMatch>(
            output_feature, chan_expr, schema::DEFAULT_TYPE, schema::ValueEncoding::RAW, "/$tsuffix"));
        rules.push_back(r);
    }

    return std::make_shared<schema::OrRules>(rules);
}

schema::Schema Talent::GetSchema() const {
    auto callee_rules = GetRules();
    auto trigger_rules = OnGetRules();

    if (!callee_rules && !trigger_rules) {
        GetLogger().Error() << "At least one callee or trigger rule must be defined";
        // TODO better error handling required
        assert(0);
    }

    if (!trigger_rules) {
        return schema_.GetSchema(callee_rules);
    }

    if (!callee_rules) {
        if (std::dynamic_pointer_cast<schema::Rules>(trigger_rules)) {
            return schema_.GetSchema(trigger_rules);
        }

        return schema_.GetSchema(OrRules(trigger_rules));
    }

    auto wrapped_trigger_rules =  OrRules(trigger_rules);

    // Exclude function outputs from the trigger rules
    std::for_each(callees_.begin(), callees_.end(), [this, &wrapped_trigger_rules](const auto& c) {
        wrapped_trigger_rules->ExcludeOn(GetOutputName(DEFAULT_TYPE, c.GetTalentId(), c.GetFunc()));
    });

    callee_rules->Add(wrapped_trigger_rules);

    return schema_.GetSchema(callee_rules);
}

void Talent::OnError(const ErrorMessage&) { }

schema::rule_ptr Talent::OnGetRules() const {
    return rules_;
}

void Talent::OnEvent(const Event& event, event_ctx_ptr context) {
    if (on_event_) {
        on_event_(event, context);
    }
}

void Talent::OnPlatformEvent(const PlatformEvent&) {}

void Talent::SetExternalEventHandler(on_event_func_ptr on_event, schema::rule_ptr rules) {
    on_event_ = on_event;
    rules_ = rules;
}

std::string Talent::GetInputName(const std::string& feature) const {
    return  feature + "-in";
}

std::string Talent::GetInputName(const std::string& talent_id, const std::string& feature) const {
    return talent_id + "." + GetInputName(feature);
}

std::string Talent::GetInputName(const std::string& type, const std::string& talent_id, const std::string& feature) const {
    return type + "." + GetInputName(talent_id, feature);
}

std::string Talent::GetOutputName(const std::string& feature) const {
    return feature + "-out";
}

std::string Talent::GetOutputName(const std::string& talent_id, const std::string& feature) const {
    return talent_id + "." + GetOutputName(feature);
}

std::string Talent::GetOutputName(const std::string& type, const std::string& talent_id, const std::string& feature) const {
    return type + "." + GetOutputName(talent_id, feature);
}

NamedLogger Talent::GetLogger() const {
    return logger_;
}

//
// FunctionTalent
//
FunctionTalent::FunctionTalent(const std::string& id)
    : Talent{id} {}

void FunctionTalent::RegisterFunction(const std::string& name, const func_ptr func) {
    funcs_[name] = func;

    AddOutput(name + "-in", schema::Metadata("Argument(s) for function " + name, 0, 0, "ONE",
                                             schema::OutputEncoding(schema::OutputEncoding::Type::Object)));
    AddOutput(name + "-out", schema::Metadata("Result of function " + name, 0, 0, "ONE",
                                              schema::OutputEncoding(schema::OutputEncoding::Type::Any)));

    auto feature_in = GetInputName(DEFAULT_TYPE, GetId(), name);
    schema_.SkipCycleCheckFor(feature_in);
}

void FunctionTalent::SkipCycleChecks() { schema_.SkipCycleChecks(); }

schema::rules_ptr FunctionTalent::GetRules() const {
    auto function_rules = std::make_shared<schema::OrRules>();
    for (auto func : funcs_) {
        auto x = std::make_unique<schema::FunctionValue>(func.first);

        auto feature = GetInputName(GetId(), func.first);
        auto constraint = std::make_unique<schema::SchemaConstraint>(feature, std::move(x), schema::DEFAULT_TYPE,
                                                                     schema::ValueEncoding::RAW);
        auto rule = std::make_shared<schema::Rule>(std::move(constraint));
        function_rules->Add(rule);
    }

    return function_rules;
}

schema::Schema FunctionTalent::GetSchema() const {
    // If no functions have been registered then our schema is no different
    // from that of a regular Talent
    if (funcs_.empty()) {
        return Talent::GetSchema();
    }

    // Generate the rules for events representing calls to our registered functions
    schema::rules_ptr call_input_rules = std::make_shared<schema::OrRules>();
    std::vector<std::string> call_input_excludes;
    std::vector<std::string> trigger_excludes;
    for (const auto& func : funcs_) {
        auto name = func.first;
        auto value = std::make_unique<schema::FunctionValue>(name);

        auto feature_in = GetInputName(GetId(), name);
        auto constraint = std::make_unique<schema::SchemaConstraint>(feature_in, std::move(value), schema::DEFAULT_TYPE,
                                                                     schema::ValueEncoding::RAW);
        auto rule = std::make_shared<schema::Rule>(std::move(constraint));
        call_input_rules->Add(rule);

        call_input_excludes.push_back(GetOutputName(schema::DEFAULT_TYPE, GetId(), name));
        trigger_excludes.push_back(GetInputName(schema::DEFAULT_TYPE, GetId(), name));
    }

    // Get the trigger rules from the subclass.
    auto trigger_rules = OnGetRules();

    // Get the rules for the events representing output from functions we call
    auto call_output_rules = Talent::GetRules();

    if (!trigger_rules && !call_output_rules) {
        // We only accept incoming function calls
        return schema_.GetSchema(call_input_rules);
    }

    auto root = call_input_rules;
    if (call_output_rules) {
        // We also accept function call output results, i.e. this
        // FunctionTalent calls other functions
        root = call_output_rules;

        call_input_rules->ExcludeOn(call_input_excludes);
        root->Add(call_input_rules);
    }

    if (trigger_rules) {
        // We also accept triggers, i.e. this FunctionTalent subscribes to
        // events.
        // If the subclass returned a single rule (i.e. not wrapped in
        // an And-Or-Rule) then we wrap it in an OrRule
        auto wrap = OnGetRules();
        schema::rules_ptr trigger_rules;
        if (wrap && !(trigger_rules = std::dynamic_pointer_cast<schema::Rules>(wrap))) {
            trigger_rules = OrRules(wrap);
        }

        trigger_rules->ExcludeOn(trigger_excludes);
        call_input_rules->Add(trigger_rules);
    }

    return schema_.GetSchema(root);
}

function_map FunctionTalent::GetFunctions() const {
    return funcs_;
}

}  // namespace core
}  // namespace iotea
