/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "iotea.hpp"

#include <chrono>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <algorithm>

#include "schema.hpp"
#include "util.hpp"

namespace iotea {
namespace core {

static constexpr char MQTT_TOPIC_NS[] = "iotea";
static constexpr char INGESTION_EVENTS_TOPIC[] = "ingestion/events";
static constexpr char PLATFORM_EVENTS_TOPIC[] = "platform/$events";
static constexpr char TALENTS_DISCOVERY_TOPIC[] = "configManager/talents/discover";

// Message
//
Message::Message(const Type msg_type, const int code)
    : msg_type_{msg_type}
    , code_{code} {}

bool Message::IsEvent() const { return msg_type_ == Message::Type::EVENT; }

bool Message::IsDiscover() const { return msg_type_ == Message::Type::DISCOVER; }

bool Message::IsError() const { return msg_type_ == Message::Type::ERROR; }

int Message::GetCode() const { return code_; }

Message Message::FromJson(const json& j) {
    auto type = j["msgType"].get<Message::Type>();

    auto code = 0;
    if (type == Message::Type::ERROR) {
        code = j["code"].get<int>();
    }

    return Message(type, code);
}

//
// DisoverMessage
//
DiscoverMessage::DiscoverMessage(const std::string& version, const std::string& return_topic)
    : version_{version}
    , return_topic_{return_topic} {}

std::string DiscoverMessage::GetVersion() const { return version_; }

std::string DiscoverMessage::GetReturnTopic() const { return return_topic_; }

DiscoverMessage DiscoverMessage::FromJson(const json& j) {
    // TODO figure out how to handle unexected message type
    assert(j["msgType"].get<Message::Type>() == Message::Type::DISCOVER);

    std::string version;

    // Compatibility to lower versions (version key has been added later)
    if (j.find("version") != j.end()) {
        version = j["version"].get<std::string>();
    } else {
        version = "0.0.0";
        log::Warn() << "Discover Message API doesn't fit the sdk version. Please update to avoid unknown behavior.";
    }

    auto return_topic = j["returnTopic"].get<std::string>();

    return DiscoverMessage{version, return_topic};
}


//
// PlatformEvent
//
PlatformEvent::PlatformEvent(const Type& type, const json& data, int64_t timestamp)
    : type_{type}
    , data_(data)
    , timestamp_{timestamp} {}

json PlatformEvent::GetData() const { return data_; }

int64_t PlatformEvent::GetTimestamp() const { return timestamp_; }

PlatformEvent::Type PlatformEvent::GetType() const { return type_; }

PlatformEvent PlatformEvent::FromJson(const json& j) {
    static const char PLATFORM_EVENT_TYPE_SET_RULES[] = "platform.talent.config.set";
    static const char PLATFORM_EVENT_TYPE_UNSET_RULES[] = "platform.talent.config.unset";

    auto type_name = j["type"].get<std::string>();

    auto type = PlatformEvent::Type::UNDEF;
    if (type_name == PLATFORM_EVENT_TYPE_SET_RULES) {
        type = PlatformEvent::Type::TALENT_RULES_SET;
    } else if (type_name == PLATFORM_EVENT_TYPE_UNSET_RULES) {
        type = PlatformEvent::Type::TALENT_RULES_UNSET;
    }

    auto data = j["data"];
    auto timestamp = j["timestamp"].get<int64_t>();

    return PlatformEvent{type, data, timestamp};
}

ErrorMessage::ErrorMessage(const int code)
    : code_{code} {}

std::string ErrorMessage::GetMessage() const {
    static const std::unordered_map<int, std::string> error_map{
        {4000, "non prefixed output feature found"},
        {4001, "feature dependency loop found"},
        {4002, "invalid discovery info"},
        {4003, "error resolving given segment in the talent ruleset"}};

    auto v = error_map.find(code_);
    if (v == error_map.end()) {
        return "unknown error";
    }

    return v->second;
}

int ErrorMessage::GetCode() const { return code_; }

ErrorMessage ErrorMessage::FromJson(const json& j) {
    auto code = j["code"].get<int>();
    return ErrorMessage{code};
}

//
// Event
//
Event::Event(const std::string& subject, const std::string& feature, const json& value, const std::string& type,
             const std::string& instance, const std::string& return_topic, int64_t when)
    : return_topic_{return_topic}
    , subject_{subject}
    , feature_{feature}
    , value_(value)
    , type_{type}
    , instance_{instance}
    , when_{when} {}

std::string Event::GetReturnTopic() const { return return_topic_; }

std::string Event::GetSubject() const { return subject_; }

std::string Event::GetFeature() const { return feature_; }

json Event::GetValue() const { return value_; }

std::string Event::GetType() const { return type_; }

std::string Event::GetInstance() const { return instance_; }

int64_t Event::GetWhen() const { return when_; }

bool Event::operator==(const Event& other) const {
    return GetSubject() == other.GetSubject()
        && GetFeature() == other.GetFeature()
        && GetValue() == other.GetValue()
        && GetType() == other.GetType()
        && GetInstance() == other.GetInstance()
        && GetReturnTopic() == other.GetReturnTopic();
}

json Event::Json() const {
    return json{{"subject", subject_}, {"feature", feature_},   {"value", value_},
                {"type", type_},       {"instance", instance_}, {"whenMs", when_}};
}

Event Event::FromJson(const json& j) {
    auto subject = j["subject"].get<std::string>();
    auto feature = j["feature"].get<std::string>();
    auto value = j["value"];
    auto type = j["type"].get<std::string>();
    auto instance = j["instance"].get<std::string>();
    auto return_topic = j.contains("returnTopic") ? j["returnTopic"].get<std::string>() : "";
    auto when_ms = j["whenMs"].get<int64_t>();

    return Event{subject, feature, value, type, instance, return_topic, when_ms};
}

OutgoingCall::OutgoingCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                           const std::string& func, const json& args, const std::string& subject,
                           const std::string& type, int64_t when)
    : talent_id_{talent_id}
    , channel_id_{channel_id}
    , call_id_{call_id}
    , func_{func}
    , args_(args)
    , subject_{subject}
    , type_{type}
    , when_{when} {}

std::string OutgoingCall::GetCallId() const { return call_id_; }

json OutgoingCall::Json() const {
    return json{{"subject", subject_},
                {"feature", talent_id_ + "." + func_ + "-in"},
                {"type", type_},
                {"value", json{
                      {"func", func_},
                      {"args", args_},
                      {"call", call_id_},
                      {"chnl", channel_id_}}
                },
                {"whenMs", when_}};
}

//
// CallToken
//
CallToken::CallToken(const std::string& call_id, int64_t timeout)
    : call_id_{call_id}
    , timeout_{timeout} {}

std::string CallToken::GetCallId() const {
    return call_id_;
}

int64_t CallToken::GetTimeout() const {
    return timeout_;
}

//
// EventContext
//
EventContext::EventContext(const std::string& talent_id, const std::string& channel_id, const std::string& subject,
                           const std::string& return_topic, reply_handler_ptr reply_handler, publisher_ptr publisher, uuid_generator_func_ptr uuid_gen)
    : talent_id_{talent_id}
    , channel_id_{channel_id}
    , subject_{subject}
    , return_topic_{return_topic}
    , reply_handler_{reply_handler}
    , publisher_{publisher}
    , uuid_gen_{uuid_gen} {}

std::string EventContext::GetChannelId() const { return channel_id_; }

std::string EventContext::GetSubject() const { return subject_; }

std::string EventContext::GetReturnTopic() const {
    // Currently the return topic sent by the platform does not contain a
    // namespace prefix so we have to add it or else the event doesn't get
    // routed properly.
    // TODO Figure out if this is a bug in the platform or not.
    //return return_topic_;

    return GetEnv(MQTT_TOPIC_NS, MQTT_TOPIC_NS) + "/" + return_topic_;
}

CallToken EventContext::Call(const Callee& callee, const json& args, const int64_t& timeout) const {
    if (!callee.IsRegistered()) {
        log::Warn() << "Tried to call unregistered Callee";

        // TODO how do we best report an error in this case? We would like to avoid using exceptions.
        return CallToken{"", -1};
    }

    auto call_id = uuid_gen_();

    auto j = args.is_array() ? args : json::array({args});
    auto c = OutgoingCall{callee.GetTalentId(), GetChannelId(), call_id, callee.GetFunc(), j, GetSubject(), callee.GetType()};

    publisher_->Publish(GetReturnTopic(), c.Json().dump());

    return CallToken{call_id, timeout};
}

int64_t EventContext::GetNowMs() const {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

//
// CallContext
//
CallContext::CallContext(const std::string& talent_id, const std::string& channel_id, const std::string& feature,
                         const Event& event, reply_handler_ptr reply_handler, publisher_ptr publisher, uuid_generator_func_ptr uuid_gen)
    : EventContext{talent_id, channel_id, event.GetSubject(), event.GetReturnTopic(), reply_handler, publisher, uuid_gen}
    , feature_{feature}
    , channel_{event.GetValue()["chnl"].get<std::string>()}
    , call_{event.GetValue()["call"].get<std::string>()} {}


void CallContext::Reply(const json& value) const {
    auto result = json{{"$tsuffix", std::string("/") + channel_ + "/" + call_}, {"$vpath", "value"}, {"value", value}};

    auto event = Event{subject_, talent_id_ + "." + feature_, result};

    // Currently the return topic sent by the platform does not contain a
    // namespace prefix so we have to add it or else the event doesn't get
    // routed properly.
    // TODO Figure out if this is a bug in the platform or not.
    auto prefixed_return_topic_ = GetEnv(MQTT_TOPIC_NS, MQTT_TOPIC_NS) + "/" + return_topic_;

    publisher_->Publish(prefixed_return_topic_, event.Json().dump());
}

//
// Gatherer
//
Gatherer::Gatherer(timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms)
    : timeout_func_{timeout_func} {

    auto smallest_timeout = int64_t{-1};

    for (const auto& t : tokens) {
        ids_.insert(t.GetCallId());

        auto token_timeout = t.GetTimeout();
        if (token_timeout > 0) {
            smallest_timeout = smallest_timeout < 0 ? token_timeout : std::min(smallest_timeout, token_timeout);
        }
    }

    if (smallest_timeout <= 0) {
        // No timeout was set
        timeout_ = 0;
        return;
    }

    timeout_ = now_ms + smallest_timeout;
}

bool Gatherer::HasTimedOut(int64_t now) const {
    if (timeout_ <= 0 || now < timeout_) {
        return false;
    }

    return true;
}

void Gatherer::TimeOut() {
    if (timeout_func_) {
        timeout_func_();
    }
}

bool Gatherer::Wants(const call_id_t& id) const {
    return ids_.find(id) != ids_.end();
}

bool Gatherer::IsReady() const {
    return ids_.size() == replies_.size();
}

bool Gatherer::Gather(const call_id_t& id, const json& reply) {
    if (ids_.find(id) == ids_.end()) {
        log::Error() << "Unrecognized call id " << id;
        return false;
    }

    replies_[id] = reply;

    return IsReady();
}

std::vector<json> Gatherer::GetReplies() const {
    std::vector<json> replies;

    for (const auto& t : ids_) {
        replies.push_back(replies_.find(t)->second);
    }

    return replies;
}

//
// SinkGatherer
//
SinkGatherer::SinkGatherer(gather_func_ptr func, timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms)
            : Gatherer{timeout_func, tokens, now_ms}
            , func_{func} {}

void SinkGatherer::ForwardReplies(const std::vector<json>& replies) const {
    func_(replies);
}

//
// PreparedFunctionReply
//
PreparedFunctionReply::PreparedFunctionReply(const std::string& talent_id,
        const std::string& feature,
        const std::string& subject,
        const std::string& channel_id,
        const std::string& call_id,
        const std::string& return_topic,
        publisher_ptr publisher)
    : talent_id_{talent_id}
    , feature_{feature}
    , subject_{subject}
    , channel_id_{channel_id}
    , call_id_{call_id}
    , return_topic_{return_topic}
    , publisher_{publisher} {}

void PreparedFunctionReply::Reply(const json& value) const {
    auto result = json{{"$tsuffix", std::string("/") + channel_id_ + "/" + call_id_}, {"$vpath", "value"}, {"value", value}};

    auto event = Event{subject_, talent_id_ + "." + feature_, result};

    // Currently the return topic sent by the platform does not contain a
    // namespace prefix so we have to add it or else the event doesn't get
    // routed properly.
    // TODO Figure out if this is a bug in the platform or not.
    auto prefixed_return_topic_ = GetEnv(MQTT_TOPIC_NS, MQTT_TOPIC_NS) + "/" + return_topic_;

    publisher_->Publish(prefixed_return_topic_, event.Json().dump());
}

//
// ReplyGatherer
//
ReplyGatherer::ReplyGatherer(gather_and_reply_func_ptr func, timeout_func_ptr timeout_func, const PreparedFunctionReply& prepared_reply, const std::vector<CallToken>& tokens, int64_t now_ms)
    : Gatherer{timeout_func, tokens, now_ms}
    , func_{func}
    , prepared_reply_{prepared_reply} {}

void ReplyGatherer::ForwardReplies(const std::vector<json>& replies) const {
    auto value = func_(replies);
    prepared_reply_.Reply(value);
}

//
// Callee
//
Callee::Callee()
    : registered_(false) {}

Callee::Callee(const std::string& talent_id, const std::string& func, const std::string& type)
    : talent_id_{talent_id}
    , func_{func}
    , type_{type} {
    registered_ = true;  // Should probably be deferred until talent is actually registered
}

std::string Callee::GetFeature() const { return talent_id_ + "." + func_; }

std::string Callee::GetFunc() const { return func_; }

std::string Callee::GetTalentId() const { return talent_id_; }

std::string Callee::GetType() const { return type_; }

bool Callee::IsRegistered() const {
    return registered_;
}

//
// ReplyHandler
//
void ReplyHandler::AddGatherer(std::shared_ptr<Gatherer> gatherer) {
    gatherers_.push_back(gatherer);
}

std::shared_ptr<Gatherer> ReplyHandler::ExtractGatherer(const call_id_t& call_id) {
    auto it = std::find_if(gatherers_.begin(), gatherers_.end(), [call_id](const auto& g) {
        return g->Wants(call_id);
    });

    if (it == gatherers_.end()) {
        return nullptr;
    }

    auto g = *it;
    gatherers_.erase(it);

    return g;
}

std::vector<std::shared_ptr<Gatherer>> ReplyHandler::ExtractTimedOut(int64_t ts) {
    std::vector<std::shared_ptr<Gatherer>> timed_out;

    auto it_end = std::remove_if(gatherers_.begin(), gatherers_.end(), [&timed_out, ts](const auto& g) {
            auto has_timed_out = g->HasTimedOut(ts);

            if (has_timed_out) {
                timed_out.push_back(g);
            }

            return has_timed_out;
        });

    gatherers_.erase(it_end, gatherers_.end());

    return timed_out;
}

//
// Talent
//
Talent::Talent(const std::string& talent_id)
    : schema_{schema::Talent{talent_id}}
    , talent_id_{talent_id} {}

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
        log::Error() << "At least one callee or trigger rule must be defined";
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

//
// Service
//
Service::Service(const std::string& name)
    : talent_{std::make_shared<FunctionTalent>(name)} {}

void Service::RegisterFunction(const std::string& name, func_ptr callback) {
    talent_->RegisterFunction(name, callback);
}

std::shared_ptr<FunctionTalent> Service::GetTalent() const { return talent_; }

//
// Client
//
Client::Client(const std::string& connection_string)
    : mqtt_client_{new MqttClient(connection_string, GenerateUUID())}
    , callee_talent_(new CalleeTalent{})
    , reply_handler_{std::make_shared<ReplyHandler>()}
    , mqtt_topic_ns_{GetEnv(MQTT_TOPIC_NS, "iotea")} {

    mqtt_client_->OnMessage = [this](mqtt::const_message_ptr msg) {
        Receive(msg->get_topic(), msg->get_payload());
    };
    mqtt_client_->OnTick = [this](const std::chrono::steady_clock::time_point ts) {
        UpdateTime(ts);
    };
}

void Client::Start() {
    callee_talent_->Initialize(reply_handler_, nullptr, GenerateUUID);
    SubscribeInternal(callee_talent_);

    static auto context_creator = [this](const std::string& subject) {
        auto ingenstion_events_topic = mqtt_topic_ns_ + "/" + INGESTION_EVENTS_TOPIC;
        return std::make_shared<EventContext>(callee_talent_->GetId(),
            callee_talent_->GetChannelId(), subject, ingenstion_events_topic,
            reply_handler_, mqtt_client_, GenerateUUID);
    };

    for (const auto& ft_pair : function_talents_) {
        ft_pair.second->Initialize(reply_handler_, context_creator, GenerateUUID);
        SubscribeInternal(ft_pair.second);
    }
    for (const auto& st_pair : subscription_talents_) {
        st_pair.second->Initialize(reply_handler_, context_creator, GenerateUUID);
        SubscribeInternal(st_pair.second);
    }

    mqtt_client_->Run();
}

void Client::Stop() {
    mqtt_client_->Stop();
}

void Client::Register(const Service& service) {
    auto t = service.GetTalent();
    function_talents_[t->GetId()] = t;
}

void Client::RegisterFunctionTalent(std::shared_ptr<FunctionTalent> t) {
    function_talents_[t->GetId()] = t;
}

void Client::RegisterTalent(std::shared_ptr<Talent> t) {
    subscription_talents_[t->GetId()] = t;
}

 void Client::SubscribeInternal(std::shared_ptr<Talent> t) {
    auto talent_id = t->GetId();
    auto channel_id = t->GetChannelId();
    auto shared_prefix = GetSharedPrefix(talent_id);

    mqtt_client_->Subscribe(shared_prefix + "/" + GetDiscoverTopic());
    mqtt_client_->Subscribe(shared_prefix + "/" + GetPlatformEventsTopic());

    mqtt_client_->Subscribe(shared_prefix + "/" + mqtt_topic_ns_ + "/talent/" + talent_id + "/events");
    mqtt_client_->Subscribe(mqtt_topic_ns_ + "/talent/" + talent_id + "/events/" + channel_id + "/+");
 }

 Callee Client::CreateCallee(const std::string& talent_id, const std::string& func, const std::string& type) {
    return callee_talent_->RegisterCallee(talent_id, func, type);
 }

void Client::Subscribe(schema::rule_ptr rules, const OnEvent callback) {
    auto t = std::make_shared<Talent>(GenerateUUID());
    t->SetExternalEventHandler(callback, rules);
    RegisterTalent(t);
}

void Client::HandleDiscover(const std::string& msg) {
    log::Debug() << "Received discovery message.";
    auto payload = json::parse(msg);
    auto dmsg = DiscoverMessage::FromJson(payload);
    auto return_topic = mqtt_topic_ns_ + "/" + dmsg.GetReturnTopic();

    callee_talent_->ClearCallees();

    for (const auto& talent : function_talents_) {
        auto callees = talent.second->GetCallees();
        callee_talent_->AddCallees(callees);

        auto schema = talent.second->GetSchema().Json().dump();
        mqtt_client_->Publish(return_topic, schema);
    }

    for (const auto& talent : subscription_talents_) {
        auto callees = talent.second->GetCallees();
        callee_talent_->AddCallees(callees);

        auto schema = talent.second->GetSchema().Json().dump();
        mqtt_client_->Publish(return_topic, schema);
    }

    if (callee_talent_->HasSchema()) {
        auto schema = callee_talent_->GetSchema().Json().dump();
        mqtt_client_->Publish(return_topic, schema);
    }
}

void Client::HandlePlatformEvent(const std::string& msg) {
    log::Debug() << "Received platform message.";
    auto payload = json::parse(msg);
    auto event = PlatformEvent::FromJson(payload);

    for (const auto& talent : function_talents_) {
        talent.second->OnPlatformEvent(event);
    }

    for (const auto& talent : subscription_talents_) {
        talent.second->OnPlatformEvent(event);
    }

    if (OnPlatformEvent) {
        OnPlatformEvent(event);
    }
}

void Client::HandleError(const ErrorMessage& err) {
    std::for_each(function_talents_.begin(), function_talents_.end(), [err](const auto& pair) {
        pair.second->OnError(err);
    });
    std::for_each(subscription_talents_.begin(), subscription_talents_.end(), [err](const auto& pair) {
        pair.second->OnError(err);
    });

    if (OnError) {
        OnError(err);
    }
}

bool Client::HandleAsCall(std::shared_ptr<FunctionTalent> t, const Event& event) {
    // Find function matching the event feature name
    auto funcs = t->GetFunctions();
    auto it = std::find_if(funcs.begin(), funcs.end(), [t, event](const auto& p) {
        return t->GetInputName(t->GetId(), p.first) == event.GetFeature();
    });

    if (it == funcs.end()) {
        // No function found
        return false;
    }

    // Invoke the callback function corresponding to the feature name
    //auto ctx = CallContext{callee_talent_->GetId(), callee_talent_->GetChannelId(), t->GetOutputName(it->first), event, reply_handler_, mqtt_client_, GenerateUUID};
    auto ctx = std::make_shared<CallContext>(t->GetId(),
            t->GetChannelId(),
            t->GetOutputName(it->first),
            event,
            reply_handler_,
            mqtt_client_,
            GenerateUUID);
    auto args = event.GetValue()["args"];
    it->second(args, ctx);
    return true;
}

void Client::HandleEvent(const std::string& talent_id, const std::string& raw) {
    Event event;

    try {
        log::Debug() << "Parse payload.";
        auto payload = json::parse(raw);

        // First check if this is an error message
        auto msg = Message::FromJson(payload);
        if (msg.IsError()) {
            log::Debug() << "Create error message from payload.";
            auto err = ErrorMessage::FromJson(payload);

            HandleError(err);
            return;
        }

        log::Debug() << "Create event from payload.";
        event = Event::FromJson(payload);
    } catch (const json::parse_error& e) {
        log::Error() << "Failed to parse event message.";
        return;
    } catch (const json::type_error& e) {
        log::Error() << "Unexpected content in event message: " << e.what();
        return;
    }

    log::Debug() << "HandleEvent, talent_id=" << talent_id << ", feature=" << event.GetFeature();

    // Is it a function talent?
    auto ft_iter = std::find_if(function_talents_.begin(), function_talents_.end(), [talent_id](const auto& item) {
        return item.first == talent_id;
    });

    if (ft_iter != function_talents_.end()) {
        // Attempt to treat the event as a function call
        if (HandleAsCall(ft_iter->second, event)) {
            return;
        }

        // It wasn't a call, treat it as an event instead
        auto ctx = std::make_shared<EventContext>(callee_talent_->GetId(),
                callee_talent_->GetChannelId(),
                event.GetSubject(),
                event.GetReturnTopic(),
                reply_handler_,
                mqtt_client_,
                GenerateUUID);
        ft_iter->second->OnEvent(event, ctx);
        return;
    }

    auto st_iter = std::find_if(subscription_talents_.begin(), subscription_talents_.end(), [talent_id](const auto& item) {
        return item.first == talent_id;
    });

    if (st_iter != subscription_talents_.end()) {
        // Found event handler
        auto t = st_iter->second;
        auto ctx = std::make_shared<EventContext>(callee_talent_->GetId(),
                callee_talent_->GetChannelId(),
                event.GetSubject(),
                event.GetReturnTopic(),
                reply_handler_,
                mqtt_client_,
                GenerateUUID);
        t->OnEvent(event, ctx);
        return;
    }

    if (callee_talent_->GetId() == talent_id) {
        auto ctx = std::make_shared<EventContext>(callee_talent_->GetId(),
                callee_talent_->GetChannelId(),
                event.GetSubject(),
                event.GetReturnTopic(),
                reply_handler_,
                mqtt_client_,
                GenerateUUID);
        callee_talent_->OnEvent(event, ctx);
        return;
    }

    log::Info() << "Received event for unregistered talent";
}

void Client::HandleCallReply(const std::string& talent_id, const std::string&
        channel_id, const call_id_t& call_id, const std::string& msg) {
    log::Debug() << "Received reply, talent_id: " << talent_id << ", channel_id=" << channel_id << " call_id=" << call_id;

    auto payload = json::parse(msg);
    auto event = Event::FromJson(payload);
    auto value = event.GetValue()["value"];

    auto gatherer = reply_handler_->ExtractGatherer(call_id);
    if (!gatherer) {
        log::Debug() << "Could not find gatherer of call id " << call_id;
        return;
    }

    gatherer->Gather(call_id, value);

    if (!gatherer->IsReady()) {
        // The gatherer expects additional replies, re-insert it.
        reply_handler_->AddGatherer(gatherer);
        return;
    }

    auto replies = gatherer->GetReplies();
    gatherer->ForwardReplies(replies);
}

std::string Client::GetDiscoverTopic() const { return mqtt_topic_ns_ + "/" + TALENTS_DISCOVERY_TOPIC; }

std::string Client::GetSharedPrefix(const std::string& talent_id) const { return "$share/" + talent_id; }

std::string Client::GetEventTopic(const std::string& talent_id) const {
    return mqtt_topic_ns_ + "/talent/" + talent_id + "/events";
}

std::string Client::GetPlatformEventsTopic() const {
    return mqtt_topic_ns_+ "/" + PLATFORM_EVENTS_TOPIC;
}

void Client::Receive(const std::string& topic, const std::string& msg) {
    log::Debug() << "Message arrived.";
    log::Debug() << "\ttopic: '" << topic << "'";
    log::Debug() << "\tpayload: '" << msg;

    std::cmatch m;

    // Forward event
    // Received events look like this {MQTT_TOPIC_NS}/talent/<talentId>/events
    // In the regex below we assume that both instance of <talentId> are the same
    static const auto event_expr = std::regex{"^" + mqtt_topic_ns_ + R"(/talent/([^/]+)/events$)"};
    if (std::regex_match(topic.c_str(), m, event_expr)) {
        auto talent_id = m[1];
        HandleEvent(talent_id, msg);
        return;
    }

    // iotea/talent/event_consumer/events/channel/callid
    // Forward deferred call response
    // talent/<talentId>/events/<callChannelId>.<deferredCallId>
    // talent/<talentId>/events/<talentId>.<callChannelId>/<callId>
    static const auto call_expr =
        std::regex{"^" + mqtt_topic_ns_ + R"(/talent/[^/]+/events/([^\.]+)\.([^/]+)/(.+)$)"};
    if (std::regex_match(topic.c_str(), m, call_expr)) {
        std::string talent_id{m[1]};
        std::string channel_id{m[2]};
        call_id_t call_id{m[3]};

        HandleCallReply(talent_id, channel_id, call_id, msg);
        return;
    }

    // Forward discovery request
    if (topic == GetDiscoverTopic()) {
        HandleDiscover(msg);
        return;
    }

    // Forward platform request
    if (topic == GetPlatformEventsTopic()) {
        HandlePlatformEvent(msg);
        return;
    }

    log::Error() << "Unexpected topic: << " << topic;
}

void Client::UpdateTime(const std::chrono::steady_clock::time_point& ts) {
    auto ts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count();
    auto timed_out = reply_handler_->ExtractTimedOut(ts_ms);

    std::for_each(timed_out.begin(), timed_out.end(), [](const auto& g) {
        g->TimeOut();
    });
}

}  // namespace core
}  // namespace iotea

