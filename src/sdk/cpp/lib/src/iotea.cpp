/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#include "iotea.hpp"

#include <chrono>
#include <memory>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <algorithm>

#include "schema.hpp"
#include "util.hpp"

namespace iotea {
namespace core {

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
PlatformEvent::PlatformEvent(const Type& type, const json& data, const timepoint_t& timestamp)
    : type_{type}
    , data_(data)
    , timestamp_{timestamp} {}

json PlatformEvent::GetData() const { return data_; }

timepoint_t PlatformEvent::GetTimestamp() const { return timestamp_; }

PlatformEvent::Type PlatformEvent::GetType() const { return type_; }

PlatformEvent PlatformEvent::FromJson(const json& j) {
    static const char PLATFORM_EVENT_TYPE_SET_RULES[] = "platform.talent.rules.set";
    static const char PLATFORM_EVENT_TYPE_UNSET_RULES[] = "platform.talent.rules.unset";

    auto type_name = j["type"].get<std::string>();

    auto type = PlatformEvent::Type::UNDEF;
    if (type_name.c_str() == PLATFORM_EVENT_TYPE_SET_RULES) {
        type = PlatformEvent::Type::TALENT_RULES_SET;
    } else if (type_name.c_str() == PLATFORM_EVENT_TYPE_UNSET_RULES) {
        type = PlatformEvent::Type::TALENT_RULES_UNSET;
    }

    auto data = j["data"];
    auto timestamp_ms = j["timestamp"].get<int64_t>();

    timepoint_t timestamp{std::chrono::milliseconds{timestamp_ms}};

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
             const std::string& instance, const std::string& return_topic, const timepoint_t& when)
    : return_topic_{return_topic}
    , subject_{subject}
    , feature_{feature}
    , value_(value)
    , type_{type}
    , instance_(instance)
    , when_(when) {}

std::string Event::GetReturnTopic() const { return return_topic_; }

std::string Event::GetSubject() const { return subject_; }

std::string Event::GetFeature() const { return feature_; }

json Event::GetValue() const { return value_; }

std::string Event::GetType() const { return type_; }

std::string Event::GetInstance() const { return instance_; }

timepoint_t Event::GetWhen() const { return when_; }

json Event::Json() const {
    auto duration = when_.time_since_epoch();
    auto when_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

    return json{{"subject", subject_}, {"feature", feature_},   {"value", value_},
                {"type", type_},       {"instance", instance_}, {"whenMs", when_ms}};
}

Event Event::FromJson(const json& j) {
    auto subject = j["subject"].get<std::string>();
    auto feature = j["feature"].get<std::string>();
    auto value = j["value"];
    auto type = j["type"].get<std::string>();
    auto instance = j["instance"].get<std::string>();
    auto return_topic = j.contains("returnTopic") ? j["returnTopic"].get<std::string>() : "";
    auto when_ms = j["whenMs"].get<int64_t>();

    timepoint_t when{std::chrono::milliseconds{when_ms}};
    return Event{subject, feature, value, type, instance, return_topic, when};
}

OutgoingCall::OutgoingCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                           const std::string& func, const json& args, const std::string& subject,
                           const std::string& type)
    : talent_id_{talent_id}
    , channel_id_{channel_id}
    , call_id_{call_id}
    , func_{func}
    , args_{args}
    , subject_{subject}
    , type_{type} {}

std::string OutgoingCall::GetCallId() const { return call_id_; }

json OutgoingCall::Json() const {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    return json{{"subject", subject_},
                {"feature", talent_id_ + "." + func_ + "-in"},
                {"type", type_},
                {"value", json{{"func", func_}, {"args", args_}, {"call", call_id_}, {"chnl", channel_id_}}},
                {"whenMs", now_ms}};
}

//
// EventContext
//
EventContext::EventContext(const std::string& talent_id, const std::string& channel_id, const std::string& subject,
                           const std::string& return_topic, call_handler_ptr call_handler, publisher_ptr publisher)
    : talent_id_{talent_id}
    , channel_id_{channel_id}
    , subject_{subject}
    , return_topic_{return_topic}
    , call_handler_{call_handler}
    , publisher_{publisher} {}

EventContext::EventContext(const std::string& talent_id, const std::string& channel_id, const Event& event,
                           call_handler_ptr call_handler, publisher_ptr publisher)
    : EventContext(talent_id, channel_id, event.GetSubject(), event.GetReturnTopic(), call_handler, publisher) {}

std::string EventContext::GetTalentId() const { return talent_id_; }

std::string EventContext::GetChannelId() const { return channel_id_; }

std::string EventContext::GetSubject() const { return subject_; }

std::string EventContext::GetReturnTopic() const { return return_topic_; }

void EventContext::Gather(gather_func_ptr func, const std::vector<call_token_t>& tokens) {
    call_handler_->Gather(func, *this, tokens);
}

//
// CallContext
//
CallContext::CallContext(const std::string& talent_id, const std::string& channel_id, const std::string& feature,
                         const Event& event, call_handler_ptr call_handler, publisher_ptr publisher)
    : EventContext{talent_id, channel_id, event, call_handler, publisher}
    , feature_{feature}
    , channel_{event.GetValue()["chnl"].get<std::string>()}
    , call_{event.GetValue()["call"].get<std::string>()} {}

void CallContext::Reply(const json& value) const {
    auto result = json{{"$tsuffix", "/" + channel_ + "/" + call_}, {"$vpath", "value"}, {"value", value}};

    auto event = Event{subject_, feature_, result};

    publisher_->Publish(return_topic_, event.Json().dump());
}

void CallContext::GatherAndReply(gather_and_reply_func_ptr func, const std::vector<call_token_t>& tokens) {
    call_handler_->GatherAndReply(func, *this, tokens);
}


//
// Callee
//
Callee::Callee()
    : registered_(false) {}

Callee::Callee(const std::string& talent_id, const std::string& func, const std::string& type, publisher_ptr publisher)
    : talent_id_{talent_id}
    , func_{func}
    , type_{type}
    , publisher_{publisher} {
    registered_ = true;  // Should probably be deferred until talent is actually registered
}

call_token_t Callee::Call(const json& args, const EventContext& ctx) const {
    if (!registered_) {
        log::Warn() << "Tried to call unregistered Callee";

        // TODO how do we best report an error in this case? We would like to avoid using exceptions.
        return static_cast<call_token_t>("");
    }

    auto call_id = GenerateUUID();

    auto j = args.is_array() ? args : json::array({args});
    auto c = OutgoingCall(talent_id_, ctx.GetChannelId(), call_id, func_, j, ctx.GetSubject(), type_);

    publisher_->Publish(ctx.GetReturnTopic(), c.Json().dump());

    return static_cast<call_token_t>(call_id);
}

std::string Callee::GetFeature() const { return talent_id_ + "." + func_; }

std::string Callee::GetFunc() const { return func_; }

std::string Callee::GetTalentId() const { return talent_id_; }

std::string Callee::GetType() const { return type_; }

//
// CallHandler
//
void CallHandler::Gather(gather_func_ptr func, const EventContext& ctx, std::vector<call_token_t> tokens) {
    auto gatherer = std::make_shared<ReplyGatherer<gather_func_ptr, EventContext>>(func, ctx, tokens);
    for (const auto& t : tokens) {
        gatherer_map_[t] = gatherer;
    }
}

void CallHandler::GatherAndReply(gather_and_reply_func_ptr func, const CallContext& ctx, std::vector<call_token_t> tokens) {
    auto gatherer = std::make_shared<ReplyGatherer<gather_and_reply_func_ptr, CallContext>>(func, ctx, tokens);
    for (const auto& t : tokens) {
        gatherer_and_reply_map_[t] = gatherer;
    }
}

void CallHandler::HandleGather(std::shared_ptr<ReplyGatherer<gather_func_ptr, EventContext>> gatherer, const call_token_t& token, const std::pair<json, EventContext>& reply) {
    gatherer->Gather(token, reply);

    if (!gatherer->IsDone()) {
        return;
    }

    auto tokens = gatherer->GetReplies();
    std::vector<std::pair<json, EventContext>> replies;
    for (const auto& t : tokens) {
        gatherer_map_.erase(t.first);
        replies.push_back(t.second);
    }

    auto func = gatherer->GetFunc();

    func(replies);
}

void CallHandler::HandleGatherAndReply(std::shared_ptr<ReplyGatherer<gather_and_reply_func_ptr, CallContext>> gatherer, const call_token_t& token, const std::pair<json, EventContext>& reply) {
    gatherer->Gather(token, reply);

    if (!gatherer->IsDone()) {
        return;
    }

    auto tokens = gatherer->GetReplies();
    std::vector<std::pair<json, EventContext>> replies;
    for (const auto& t : tokens) {
        gatherer_and_reply_map_.erase(t.first);
        replies.push_back(t.second);
    }

    auto func = gatherer->GetFunc();
    auto ctx = gatherer->GetContext();

    auto freply = func(replies);
    ctx.Reply(freply);
}

void CallHandler::HandleReply(const call_token_t& token, const std::pair<json, EventContext>& reply) {
    auto g_entry = gatherer_map_.find(token);
    if (g_entry != gatherer_map_.end()) {
        HandleGather(g_entry->second, token, reply);
        return;
    }

    auto gar_entry = gatherer_and_reply_map_.find(token);
    if (gar_entry != gatherer_and_reply_map_.end()) {
        HandleGatherAndReply(gar_entry->second, token, reply);
        return;
    }

    log::Warn() << "Could not find gatherer of call id " << token;
}


//
// Talent
//
Talent::Talent(const std::string& talent_id, publisher_ptr publisher)
    : talent_id_{talent_id}
    , channel_id_{GenerateUUID()}
    , publisher_{publisher}
    , schema_{schema::Talent{talent_id_}}
    , call_handler_{std::make_shared<CallHandler>()} {}

std::string Talent::GetId() const { return talent_id_; }

Callee Talent::CreateCallee(const std::string& talent_id, const std::string& func, const std::string& type) {
    auto c = Callee(talent_id, func, type, publisher_);
    callees_.insert(c.GetFeature());
    return c;
}

std::string Talent::GetChannelId() const { return channel_id_; }

publisher_ptr Talent::GetPublisher() const { return publisher_; }

void Talent::AddOutput(const std::string& feature, const schema::Metadata& metadata) {
    schema_.AddOutput(feature, metadata);
}

EventContext Talent::NewEventContext(const std::string& subject) {
    return EventContext{talent_id_, channel_id_, subject, publisher_->GetIngestionTopic(), call_handler_, publisher_};
}

schema::rules_ptr Talent::GetRules() const {
    if (callees_.empty()) {
        return nullptr;
    }

    auto rules = schema::rule_vec{};
    auto chan_expr = "^/" + channel_id_ + "/.*";

    for (const auto& c : callees) {
        auto r = std::make_shared<schema::Rule>(std::make_shared<schema::RegexMatch>(
            c + "-out", chan_expr, schema::DEFAULT_TYPE, schema::ValueEncoding::RAW, "/$tsuffix"));
        rules.push_back(r);
    }

    return std::make_shared<schema::OrRules>(rules);
}

schema::Schema Talent::GetSchema() const {
    auto rules = GetRules();
    auto external_rules = OnGetRules();

    if (rules) {
        if (external_rules) {
            rules->Add(external_rules);
        }
    } else if (external_rules) {
        rules = external_rules;
    }

    // TODO what to do if there are no internal and no external rules?

    return schema_.GetSchema(rules);
}

void Talent::HandleEvent(const std::string& data) {
    try {
        log::Debug() << "Parse payload.";
        auto payload = json::parse(data);

        // First check if this is an error message
        auto msg = Message::FromJson(payload);
        if (msg.IsError()) {
            log::Debug() << "Create error message from payload.";
            OnError(ErrorMessage::FromJson(payload));
            return;
        }

        log::Debug() << "Create event from payload.";
        auto event = Event::FromJson(payload);
        HandleEvent(event);
    } catch (const json::parse_error& e) {
        log::Error() << "Failed to parse event message.";
    } catch (const json::type_error& e) {
        log::Error() << "Unexpected content in event message: " << e.what();
    }
}

void Talent::HandleEvent(const Event& event) {
    auto context = EventContext{talent_id_, channel_id_, event, call_handler_, publisher_};
    OnEvent(event, context);
}

void Talent::OnError(const ErrorMessage& msg) { (void)msg; }

schema::rules_ptr Talent::OnGetRules() const { return nullptr; }

void Talent::OnEvent(const Event& event, EventContext context) {
    (void)event;
    (void)context;
}

void Talent::OnPlatformEvent(const PlatformEvent& event) {
    (void)event;
}

void Talent::HandleDiscover(const std::string& data) {
    try {
        auto payload = json::parse(data);
        auto dmsg = DiscoverMessage::FromJson(payload);
        auto return_topic = publisher_->GetNamespace() + "/" + dmsg.GetReturnTopic();
        auto response = GetSchema().Json().dump();

        log::Debug() << "Publishing response.";
        log::Debug() << "\ttopic: '" << return_topic << "'";
        log::Debug() << "\tpayload: '" << response << "'\n";
        publisher_->Publish(return_topic, response);
    } catch (const json::parse_error& e) {
        log::Error() << "Failed to parse discovery message.";
    } catch (const json::type_error& e) {
        log::Error() << "Unexpected content in discovery message: " << e.what();
    }
}

void Talent::HandlePlatformEvent(const std::string& data) {
    try {
        auto payload = json::parse(data);
        auto event = PlatformEvent::FromJson(payload);

        OnPlatformEvent(event);
    } catch (const json::parse_error& e) {
        log::Error() << "Failed to parse platform event.";
    } catch (const json::type_error& e) {
        log::Error() << "Unexpected content in platform event: " << e.what();
    }
}

void Talent::HandleReply(const std::string& channel_id, const std::string& call_id, const std::string& data) {
    if (channel_id_ != channel_id) {
        log::Info() << "Unexpected channel id " << channel_id;
        return;
    }

    auto payload = json::parse(data);
    auto event = Event::FromJson(payload);
    auto value = event.GetValue()["value"];
    auto context = EventContext(talent_id_, channel_id , event, call_handler_, publisher_);

    call_handler_->HandleReply(static_cast<call_token_t>(call_id), std::make_pair(value, context));
}

call_handler_ptr Talent::GetCallHandler() const { return call_handler_; }


//
// FunctionTalent
//
FunctionTalent::FunctionTalent(const std::string& id, publisher_ptr publisher)
    : Talent{id, publisher} {}

void FunctionTalent::RegisterFunction(const std::string& name, const func_ptr func) {
    funcs_[name] = func;

    AddOutput(name + "-in", schema::Metadata("Argument(s) for function " + name, "ONE",
                                             schema::OutputEncoding(schema::OutputEncoding::Type::Object)));
    AddOutput(name + "-out", schema::Metadata("Results of function " + name, "ONE",
                                              schema::OutputEncoding(schema::OutputEncoding::Type::Any)));
}

void FunctionTalent::SkipCycleCheck(bool skip) { schema_.SkipCycleCheck(skip); }

schema::rules_ptr FunctionTalent::GetRules() const {
    auto talent_rules = Talent::GetRules();

    schema::rule_vec rules;
    for (auto func : funcs_) {
        auto x = std::make_unique<schema::FunctionValue>(func.first);

        // auto feature = id_ + "." + func.first + "-in";
        auto feature = GetInputName(func.first);
        auto constraint = std::make_unique<schema::SchemaConstraint>(feature, std::move(x), schema::DEFAULT_TYPE,
                                                                     schema::ValueEncoding::RAW);
        auto rule = std::make_shared<schema::Rule>(std::move(constraint));
        rules.push_back(rule);
    }

    if (talent_rules) {
        talent_rules->Add(rules);
    } else {
        talent_rules = std::make_shared<schema::OrRules>(rules);
    }
    return talent_rules;
}

void FunctionTalent::HandleEvent(const Event& event) {
    auto feature = event.GetFeature();

    auto item = std::find_if(funcs_.begin(), funcs_.end(), [this, feature](const std::pair<std::string, func_ptr>& p) {
        return GetInputName(p.first) == feature;
    });

    if (item == funcs_.end()) {
        Talent::HandleEvent(event);
        return;
    }

    auto args = event.GetValue()["args"];
    auto context = CallContext{static_cast<Talent&>(*this), publisher_, GetOutputName(item->first), event};
    item->second(args, context);
}

std::string FunctionTalent::GetInputName(const std::string& feature) const { return GetId() + "." + feature + "-in"; }

std::string FunctionTalent::GetOutputName(const std::string& feature) const { return GetId() + "." + feature + "-out"; }

}  // namespace core
}  // namespace iotea
