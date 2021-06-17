/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <algorithm>

#include "event.hpp"
#include "client.hpp"
#include "logging.hpp"

namespace iotea {
namespace core {

static constexpr char INGESTION_EVENTS_TOPIC[] = "ingestion/events";
static constexpr char PLATFORM_EVENTS_TOPIC[] = "platform/$events";
static constexpr char TALENTS_DISCOVERY_TOPIC[] = "configManager/talents/discover";

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
// CalleeTalent
//
CalleeTalent::CalleeTalent(const std::string& id)
     : Talent(id) {}

Callee CalleeTalent::RegisterCallee(const std::string& talent_id, const std::string& func, const std::string& type) {
    auto c = Callee{talent_id, func, type};
    internal_callees_.push_back(c);
    return c;
}

bool CalleeTalent::HasSchema() const {
    return !(internal_callees_.empty() && callees_.empty());
}

void CalleeTalent::ClearCallees() {
    callees_.clear();
    callees_.insert(callees_.begin(), internal_callees_.begin(), internal_callees_.end());
}

void CalleeTalent::AddCallees(const std::vector<Callee>& callees) {
    callees_.insert(callees_.begin(), callees.begin(), callees.end());
}


//
// Client
//
static auto logger = NamedLogger("Client");

Client::Client(const std::string& connection_string)
    : mqtt_client_{new MqttClient(connection_string, GenerateUUID())}
    , callee_talent_(new CalleeTalent{GenerateUUID()})
    , reply_handler_{std::make_shared<ReplyHandler>()}
    , mqtt_topic_ns_{GetEnv(MQTT_TOPIC_NS, "iotea")} {

    mqtt_client_->OnMessage = [this](mqtt::const_message_ptr msg) {
        Receive(msg->get_topic(), msg->get_payload());
    };
    mqtt_client_->OnTick = [this](int64_t ts) {
        UpdateTime(ts);
    };
}

Client::Client(std::shared_ptr<MqttClient> mqtt_client,
        std::shared_ptr<CalleeTalent> callee_talent,
        reply_handler_ptr reply_handler,
        const std::string& mqtt_topic_ns)
    : mqtt_client_{mqtt_client}
    , callee_talent_{callee_talent}
    , reply_handler_{reply_handler}
    , mqtt_topic_ns_{mqtt_topic_ns} {}


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
    logger.Debug() << "Received discovery message.";
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
    logger.Debug() << "Received platform message.";
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
        logger.Debug() << "Parse payload.";
        auto payload = json::parse(raw);

        // First check if this is an error message
        auto msg = Message::FromJson(payload);
        if (msg.IsError()) {
            logger.Debug() << "Create error message from payload.";
            auto err = ErrorMessage::FromJson(payload);

            HandleError(err);
            return;
        }

        logger.Debug() << "Create event from payload.";
        event = Event::FromJson(payload);
    } catch (const json::parse_error& e) {
        logger.Error() << "Failed to parse event message.";
        return;
    } catch (const json::type_error& e) {
        logger.Error() << "Unexpected content in event message: " << e.what();
        return;
    }

    logger.Debug() << "HandleEvent, talent_id=" << talent_id << ", feature=" << event.GetFeature();

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

    logger.Info() << "Received event for unregistered talent";
}

void Client::HandleCallReply(const std::string& talent_id, const std::string&
        channel_id, const call_id_t& call_id, const std::string& msg) {
    logger.Debug() << "Received reply, talent_id: " << talent_id << ", channel_id=" << channel_id << " call_id=" << call_id;

    auto payload = json::parse(msg);
    auto event = Event::FromJson(payload);
    auto value = event.GetValue()["value"];

    auto gatherer = reply_handler_->ExtractGatherer(call_id);
    if (!gatherer) {
        logger.Debug() << "Could not find gatherer of call id " << call_id;
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
    logger.Debug() << "Message arrived.";
    logger.Debug() << "\ttopic: '" << topic << "'";
    logger.Debug() << "\tpayload: '" << msg;

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

    logger.Error() << "Unexpected topic: << " << topic;
}

void Client::UpdateTime(int64_t ts) {
    auto timed_out = reply_handler_->ExtractTimedOut(ts);

    std::for_each(timed_out.begin(), timed_out.end(), [](const auto& g) {
        g->TimeOut();
    });
}

}  // namespace core
}  // namespace iotea
