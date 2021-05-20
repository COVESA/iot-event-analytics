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

#include <limits>
#include <regex>
#include <utility>
#include <vector>
#include <algorithm>

#include "logging.hpp"
#include "schema.hpp"
#include "util.hpp"

using iotea::core::logging::NamedLogger;

namespace iotea {
namespace core {

static auto logger = NamedLogger{"Message"};

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
        logger.Warn() << "Discover Message API doesn't fit the sdk version. Please update to avoid unknown behavior.";
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


}  // namespace core
}  // namespace iotea

