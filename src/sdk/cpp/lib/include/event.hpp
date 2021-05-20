/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_EVENT_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_EVENT_HPP_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <functional>

#include "nlohmann/json.hpp"

#include "common.hpp"
#include "interface.hpp"
#include "logging.hpp"
#include "schema.hpp"
#include "util.hpp"

using json = nlohmann::json;

namespace iotea {
namespace core {

#if 0
class Message;
class Talent;
class Event;
class Publisher;
class CallToken;
class EventContext;
class CallContext;
class Callee;
class ReplyHandler;
class DiscoverMessage;
#endif

/**
 * @brief Arriving messages must be parsed in two steps beginning with
 * determining what kind of message it is (one of DiscoverMessage, Event,
 * ErrorMessage). Message serves as an intermediate representation of the
 * message used to determine how the final message should be decoded and
 * routed. Should not be created by external clients.
 *
 */
class Message {
   public:
    enum class Type {
        EVENT = 1,    // 2^0
        DISCOVER = 2, // 2^1
        ERROR = 4     // 2^2
    };

   public:
    /**
     * @brief Construct a new Message object.
     *
     * @param msg_type The type of message
     * @param code The error code, only relevant if the type is Message::Type::ERROR
     */
    explicit Message(const Type msg_type, const int code = 0);

    /**
     * @brief Test if this is an event message.
     *
     * @return true If this is an event message
     * @return false If this is not an event message
     */
    bool IsEvent() const;

    /**
     * @brief Test if this is a discover message.
     *
     * @return true If this is a discover message
     * @return false If this is not a discover message
     */
    bool IsDiscover() const;

    /**
     * @brief Test if this is an error message.
     *
     * @return true If this is an error message
     * @return false If this is not an error message
     */
    bool IsError() const;

    /**
     * @brief Get the error code associated with this message. Only relevant if
     * IsError() returns true.
     *
     * @return int The error code if the message represents an error
     * @return int 0 if the message doesn't represent an error
     */
    int GetCode() const;

    /**
     * @brief Create a Message from JSON.
     *
     * @param j The JSON prepresentation of a Message.
     * @return Message
     */
    static Message FromJson(const json& j);

   protected:
    enum Type msg_type_ = Type::EVENT;
    int code_;
};

/**
 * @brief DiscoverMessage is periodically sent by the platform in order to
 * trigger Talents to reply with a rule set outlining Talent properties, what
 * it produces and what it consumes. When a DiscoveryMessage is received
 * Talent::OnGetRules() is called in order to fetch Talent specific rules.
 * Should not be created by external clients.
 *
 */
class DiscoverMessage {
   public:
    /**
     * @brief Construct a new DiscoverMessage object.
     *
     * @param version The version of the message
     * @param return_topic The name of the topic to reply to.
     */
    DiscoverMessage(const std::string& version, const std::string& return_topic);

    /**
     * @brief Get the DiscoverMessage version.
     *
     * @return std::string
     */
    std::string GetVersion() const;

    /**
     * @brief Get the return topic.
     *
     * @return std::string
     */
    std::string GetReturnTopic() const;

    /**
     * @brief Create a DiscoverMessage from JSON.
     *
     * @param j The JSON representation of a DiscoverMessage
     * @return DiscoverMessage
     */
    static DiscoverMessage FromJson(const json& j);

   private:
    const std::string version_;
    const std::string return_topic_;

};

class PlatformEvent {
   public:
    enum class Type {
        TALENT_RULES_SET,
        TALENT_RULES_UNSET,
        UNDEF
    };

   public:
    /**
     * @brief Construct a new PlatformEvent.
     *
     * @param type The event type
     * @param data The JSON representation of the PlatformEvent
     * @param timestamp The time when the event was emitted in ms since the epoch.
     */
    PlatformEvent(const Type& type, const json& data, int64_t timestamp);

    /**
     * @brief Get the data attached to the event.
     *
     * @return json
     */
    json GetData() const;

    /**
     * @brief Get the time when the event was emitted.
     *
     * @return int64_t
     */
    int64_t GetTimestamp() const;

    /**
     * @brief Get the event type.
     *
     * @return PlatformEvent::Type
     */
    Type GetType() const;

    /**
     * @brief Create a PlatformEvent from JSON.
     *
     * @return PlatformEvent
     */
    static PlatformEvent FromJson(const json& j);

   private:
    Type type_;
    json data_;
    int64_t timestamp_;
};

/**
 * @brief ErrorMessage is returned by the platform when something has gone
 * wrong. Should not be created by external clients.
 *
 */
class ErrorMessage {
   public:
    /**
     * @brief Construct a new ErrorMessage object
     *
     * @param code A numerical error code
     */
    explicit ErrorMessage(const int code);

    /**
     * @brief Get a description of the error.
     *
     * @return std::string
     */
    std::string GetMessage() const;

    /**
     * @brief Get the error code.
     *
     * @return int
     */
    int GetCode() const;

    /**
     * @brief Create an ErrorMessage from JSON.
     *
     * @param j The JSON representation of an ErrorMessage
     * @return ErrorMessage
     */
    static ErrorMessage FromJson(const json& j);

   private:
    const int code_;
};

/**
 * @brief Event represents an incoming event. Should not be used by external clients.
 *
 */
class Event {
   public:
    /**
     * @brief Construct a new Event object
     *
     * @param subject The name of the subject as determined by the context from which the event originated.
     * @param feature The name of the feature represented by the event.
     * @param value The event payload value.
     * @param type The name of the type associated with the event.
     * @param instance The name of the instance associated with the event.
     * @param return_topic The name of the topic to send replies on (if any).
     * @param when The point in time when the event was emitted.
     */
    Event(const std::string& subject, const std::string& feature, const json& value,
          const std::string& type = "default", const std::string& instance = "default",
          const std::string& return_topic = "", int64_t when = GetEpochTimeMs());

    Event() = default;

    /**
     * @brief Get the name of the return topic.
     *
     * @return std::string
     */
    virtual std::string GetReturnTopic() const;

    /**
     * @brief Get the name of the subject.
     *
     * @return std::string
     */
    virtual std::string GetSubject() const;

    /**
     * @brief Get the name of the feature.
     *
     * @return std::string
     */
    virtual std::string GetFeature() const;

    /**
     * @brief Get the payload value as JSON.
     *
     * @return json
     */
    virtual json GetValue() const;

    /**
     * @brief Get the name of the instance.
     *
     * @return std::string
     */
    virtual std::string GetInstance() const;

    /**
     * @brief Get the name of the type.
     *
     * @return std::string
     */
    virtual std::string GetType() const;

    /**
     * @brief Get the time when the event was emitted in milliseconds since the
     * epoch.
     *
     * @return int64_t
     */
    virtual int64_t GetWhen() const;

    /**
     * @brief Get a representation of the event as JSON.
     *
     * @return json
     */
    virtual json Json() const;

    /**
     * @brief Create an event from JSON.
     *
     * @param j The JSON representaion of an event.
     * @return Event
     */
    static Event FromJson(const json& j);

    /**
     * @brief Compare this Event to another. Comparison ignores the "when_" member.
     *
     * @param other The Event to compare this Event to
     *
     * @return true If this Event is equal to "other"
     */
    bool operator==(const Event& other) const;

   private:
    std::string return_topic_;
    std::string subject_;
    std::string feature_;
    json value_;
    std::string type_;
    std::string instance_;
    int64_t when_;
};

/**
 * @brief OutgoingEvent contains all the necessary information required to emit
 * an event. Should not be used by external clients.
 *
 * @tparam T The type of the event payload value
 */
template <typename T>
class OutgoingEvent {
   public:
    /**
     * @brief Construct an OutgoingEvent object
     *
     * @param subject The name of the subject as determined by the context from which the event is emitted
     * @param talent The id of talent emitting the event
     * @param feature The name of feature emitting the event
     * @param value The event payload value
     * @param type The name of the type associated with the event
     * @param instance The name of the instance producing the event
     * @param when The time since the epoch in ms to attach to the event
     */
    OutgoingEvent(const std::string& subject, const std::string& talent_id, const std::string& feature, const T& value, const std::string& type,
                  const std::string& instance, int64_t when = GetEpochTimeMs())
        : subject_{subject}
        , talent_id_{talent_id}
        , feature_{feature}
        , value_{value}
        , type_{type}
        , instance_{instance}
        , when_{when} {}

    /**
     * @brief Get a JSON representation of the event.
     *
     * @return json
     */
    json Json() const {
        return json{
            {"subject", subject_},
            {"feature", feature_},
            {"value", value_},
            {"type", type_},
            {"instance", instance_},
            {"whenMs", when_}};
    }

   private:
    std::string subject_;
    std::string talent_id_;
    std::string feature_;
    T value_;
    std::string type_;
    std::string instance_;
    int64_t when_;
};


}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_EVENT_HPP_
