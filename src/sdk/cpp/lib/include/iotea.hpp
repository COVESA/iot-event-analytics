/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef IOTEA_IOTEA_HPP
#define IOTEA_IOTEA_HPP

#include <chrono>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <functional>

#include "nlohmann/json.hpp"

#include "interface.hpp"
#include "mqtt_client.hpp"
#include "logging.hpp"
#include "schema.hpp"
#include "util.hpp"

using json = nlohmann::json;

namespace iotea {
namespace core {

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

using call_id_t = std::string;
using event_ctx_ptr = std::shared_ptr<EventContext>;
using call_ctx_ptr = std::shared_ptr<CallContext>;
using func_ptr = std::function<void(const json&, call_ctx_ptr)>;
using function_map = std::unordered_map<std::string, func_ptr>;
using gather_func_ptr = std::function<void(std::vector<json>)>;
using gather_and_reply_func_ptr = std::function<json(std::vector<json>)>;
using timeout_func_ptr = std::function<void(void)>;
using talent_ptr = std::shared_ptr<Talent>;
using talent_map = std::unordered_map<std::string, talent_ptr>;
using publisher_ptr = std::shared_ptr<Publisher>;
using reply_handler_ptr = std::shared_ptr<ReplyHandler>;


using context_generator_func_ptr = std::function<event_ctx_ptr(const std::string& subject)>;
using uuid_generator_func_ptr = std::function<std::string(void)>;
using on_event_func_ptr = std::function<void(const Event&, event_ctx_ptr)>;

static constexpr char DEFAULT_INSTANCE[] = "default";
static constexpr char DEFAULT_TYPE[] = "default";

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
     * @brief Compare this event to another event. Comparison ignores the "when_" member.
     *
     * @param other The event to compare this event to
     *
     * @return true If this event is equal to "other"
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
#if 0
        return json{{"subject", subject_}, {"feature", talent_id_ + "." + feature_},   {"value", value_},
                    {"type", type_},       {"instance", instance_}, {"whenMs", when_}};
#endif
        return json{{"subject", subject_}, {"feature", feature_},   {"value", value_},
                    {"type", type_},       {"instance", instance_}, {"whenMs", when_}};
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

/**
 * @brief OutgoingCall contains all the necessary information required to
 * perform a function call. Should not be used by external clients.
 *
 */
class OutgoingCall {
   public:
   /**
    * @brief Construct a new OutgoingCall object
    *
    * @param talent_id The ID of the Talent providing the function
    * @param channel_id The ID of the channel determined by the context from which the call is made
    * @param call_id The unique ID of the call provided by the caller
    * @param func The name of the function to call
    * @param args The arguments to pass to the function
    * @param subject The name of subject as determined by the context from which the call is made
    * @param type The name of the type associated with the called function
    * @param when The time since the epoch in ms to attach to the call
    */
    OutgoingCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                 const std::string& func, const json& args, const std::string& subject, const std::string& type,
                 int64_t when = GetEpochTimeMs());

    /**
     * @brief Get a JSON representation of this OutgoingCall
     *
     * @return json
     */
    json Json() const;

    /**
     * @brief Get the ID of the call.
     *
     * @return std::string
     */
    std::string GetCallId() const;

   private:
    std::string talent_id_;
    std::string channel_id_;
    std::string call_id_;
    std::string func_;
    json args_;
    std::string subject_;
    std::string type_;
    int64_t when_;
};

/**
 * @brief A Callee represents a callable function provided by some Talent. A
 * Callee should not be created directly but through Talent::GetCallee().
 */
class Callee {
   public:
    /**
     * @brief Construct a new Callee. Clients should not call this constructor
     * directly but should use Talent::CreateCallee().
     *
     * @param talent_id The ID of the Talent providing the function
     * @param func The name of the function to call
     * @param type The name of the type associated with the function
     */
    Callee(const std::string& talent_id, const std::string& func, const std::string& type);

   /**
    * @brief Construct a new Callee.
    *
    */
    Callee();

    /**
     * @brief Get the name of the feature providing the function call.
     *
     * @return std::string
     */
    std::string GetFeature() const;

    /**
     * @brief Get the name of the function.
     *
     * @return std::string
     */
    std::string GetFunc() const;

    /**
     * @brief Get the ID of the Talent providing the function.
     *
     * @return std::string
     */
    std::string GetTalentId() const;

    /**
     * @brief Get the name of the type associated with the function.
     *
     * @return std::string
     */
    std::string GetType() const;

    /**
     * @brief Return whether the function represented by the Callee is
     * currently registered with the platform.
     *
     * @return true if the function is registered.
     */
    bool IsRegistered() const;

   private:
    std::string talent_id_;
    std::string func_;
    std::string type_;
    bool registered_;
};


/**
 * @brief CallToken represents a pending function call.
 */
class CallToken {
    public:
     /**
      * @brief Constructs a new CallToken.
      *
      * @param call_id A unique ID
      * @param timeout The maximum time to wait for a reply to a call in ms. If
      * the call can remain pending indefinatly use 0 (will leak memory if the
      * reply is never received).
      */
     explicit CallToken(const call_id_t& call_id, int64_t timeout = 0);

     /**
      * @brief Get the ID of the call.
      *
      * @return std::string
      */
     call_id_t GetCallId() const;

     /**
      * @brief Get the call timeout.
      *
      * @return int64_t
      */
     int64_t GetTimeout() const;

    private:
     call_id_t call_id_;
     int64_t timeout_;
};

/**
 * @brief Gatherer associates a set of pending replies with a callback
 * function.
 */
class Gatherer {
   public:
    /**
     * @brief Constructs a new Gatherer.
     *
     * @param timeout_func_ptr A pointer to a function to call if any of the expected replies times out
     * @param tokens A collection of CallTokens representing pending replies to gather
     * @param now_ms The notion of now in ms
     */
    Gatherer(timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms = 0);

    virtual ~Gatherer() = default;

    /**
     * @brief Check if any of the pending calls have timed out.
     *
     * @return true if at least one call has timed out.
     */
    virtual bool HasTimedOut(int64_t now) const;

    /**
     * @brief Return whether the gatherer expects a particular call ID.
     *
     * @param id A pending call ID.
     * @return true if the gatherer expects the ID.
     */
    virtual bool Wants(const call_id_t& id) const;

    /**
     * @brief Return whether the gatherer has gathered all the expected replies
     * and is ready for forward them.
     *
     * @return bool
     */
    virtual bool IsReady() const;

    /**
     * @brief Gather the provided ID and the reply to the associated call.  If
     * all the expected IDs have been gathered the virtual function Call is
     * called.
     *
     * @param id A pending call ID.
     * @param reply The reply to the call.
     * @return true if the all the expected IDs have been gathered
     */
    virtual bool Gather(const call_id_t& id, const json& reply);


    /**
     * @brief Get the replies gathered so far.
     *
     * @return std::vector<json>
     */
    virtual std::vector<json> GetReplies() const;

    /**
     * @brief ForwardReplies must be overloaded by subclasses and should be
     * called if and when all the expected IDs have been gathered.
     *
     * @param replies A vector of replies
     */
    virtual void ForwardReplies(const std::vector<json>& replies) const = 0;

    /**
     * @brief Call the registerd timeout function. Should be called if and when
     * the pending calls have timed out.
     */
    virtual void TimeOut();

   protected:
    timeout_func_ptr timeout_func_;
    std::set<call_id_t> ids_;
    std::unordered_map<call_id_t, json> replies_;
    int64_t timeout_;

};


/**
 * @brief SinkGatherer gathers a collection of pending replies, exectues a
 * callback function but does not produce a reply.
 */
class SinkGatherer : public Gatherer {
   public:
    /**
     * @brief Construct a new SinkGatherer.
     *
     * @param func A function to call once all the required tokens have been
     * gathered.
     * @param timeout_func A function to call if any of the calls times before
     * yielding a reply.
     * @param tokens A vector of tokens as returned by
     * EventContext::Call. The order of the replies forwared to func matches the
     * order of the tokens.
     * @param now_ms The notion of now in ms. If now_ms <= 0 the steady clock
     * is used go fetch the current time. Provided as an argument so that tests
     * can override it.
     */
    SinkGatherer(gather_func_ptr func, timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms = 0);

    virtual void ForwardReplies(const std::vector<json>& replies) const override;

   private:
    gather_func_ptr func_;
};

/**
 * @brief PreparedFunctionReply holds everything necessary to send a function
 * reply back to the caller except the actual function result.
 */
class PreparedFunctionReply {
   public:
    /**
     * @brief Construct a new PreparedFunctionReply object.
     *
     * @param talent_id The ID of the talent to send the reply to
     * @param feature The name of the feature
     * @param subject The subject of the original event resulting in the
     * function call
     * @param channel_id The ID of the calling talent's channel_id
     * @param call_id The unique call IDs
     * @param return_topic The topic on which the reply should be posted
     * @param publisher A pointer to a Publisher
     */
    PreparedFunctionReply(const std::string& talent_id,
            const std::string& feature,
            const std::string& subject,
            const std::string& channel_id,
            const std::string& call_id,
            const std::string& return_topic,
            publisher_ptr publisher);

    virtual ~PreparedFunctionReply() = default;

    /**
     * @brief Send the reply.
     *
     * @param value The function reply value
     */
    virtual void Reply(const json& value) const;

   private:
    std::string talent_id_;
    std::string feature_;
    std::string subject_;
    std::string channel_id_;
    std::string call_id_;
    std::string return_topic_;
    publisher_ptr publisher_;
};

/**
 * @brief ReplyGatherer gathers a collection of pending replies, exectues a
 * callback function and sends back a reply.
 */
class ReplyGatherer : public Gatherer {
   public:
    /**
     * @brief Construct a new ReplyGatherer.
     *
     * @param func A function to call once all the required tokens have been
     * gathered.
     * @param timeout_func A function to call if any of the calls times before
     * yielding a reply.
     * @param tokens A vector of tokens as returned by
     * EventContext::Call. The order of the replies forwared to func matches the
     * order of the tokens.
     * @param now_ms The notion of now in ms. If now_ms <= 0 the steady clock
     * is used go fetch the current time. Provided as an argument so that tests
     * can override it.
     */
    ReplyGatherer(gather_and_reply_func_ptr func, timeout_func_ptr timeout_func, const PreparedFunctionReply& prepared_reply, const std::vector<CallToken>& tokens, int64_t now_ms = 0);

    virtual void ForwardReplies(const std::vector<json>& replies) const override;

   private:
    gather_and_reply_func_ptr func_;
    PreparedFunctionReply prepared_reply_;
};


/**
 * @brief ReplyHandler maintains a collection of pending function calls and
 * their associated result handling functions. ReplyHandler should not be used
 * by external clients.
 *
 */
class ReplyHandler {
   public:
    virtual ~ReplyHandler() = default;

    /**
     * @brief Add a Gatherer to this ReplyHandler.
     *
     * @param gatherer The Gatherer
     */
    void AddGatherer(std::shared_ptr<Gatherer> gatherer);

    /**
     * @brief Remove and return the Gatherer matching id (if any).
     *
     * @param id A call ID.
     * @return std::shared_ptr<Gatherer> or nullptr if no matching Gatherer was
     * found.
     */
    std::shared_ptr<Gatherer> ExtractGatherer(const call_id_t& id);

    /**
     * @brief Remove and return all Gatherers that are waiting for a reply that
     * has timed out.
     *
     * @param ts A steady clock timestamp in ms
     * @return std::vector<std::shard_ptr<Gatherer>>
     */
    std::vector<std::shared_ptr<Gatherer>> ExtractTimedOut(int64_t ts);

   private:
    std::vector<std::shared_ptr<Gatherer>> gatherers_;
};


/**
 * @brief EventContext is the context within which a set of events and calls
 * originated. The purpose of the context is to be able to trace a chain of
 * events and calls.
 */
class EventContext {
   public:
    virtual ~EventContext() = default;

    /**
     * @brief Construct a new EventContext. External clients should not call
     * this constructor explicitly but should instead use
     * Talent.NewEventContext().
     *
     * @param talent_id The id of the Talent for which this context exists
     * @param channel_id The unique channel ID of the Talent
     * @param subject The subject of the context
     * @param return_topic The name of the topic to reply to
     * @param reply_handler The ReplyHandler to use for collecting replies to outgoing calls
     * @param publisher A publisher to send replies with
     * @param uuid_gen A function generating stringified UUID4s
     */
    EventContext(const std::string& talent_id, const std::string& channel_id, const std::string& subject,
                 const std::string& return_topic, reply_handler_ptr reply_handler, publisher_ptr publisher, uuid_generator_func_ptr uuid_gen);

    /**
     * @brief Get the ID of the Talent.
     *
     * @return std::string
     */
    std::string GetTalentId() const;

    /**
     * @brief Get the channel ID of the Talent.
     *
     * @return std::string
     */
    std::string GetChannelId() const;

    /**
     * @brief Get the subject.
     *
     * @return std::string
     */
    std::string GetSubject() const;

    /**
     * @brief Get the return topic.
     *
     * @return std::string
     */
    std::string GetReturnTopic() const;

    /**
     * @brief Emit an event within this context.
     *
     * @code
       auto degrees = collect_temperature();
       context.Emit("temperature", degrees, "device");
     * @endcode
     *
     * @tparam T The value to emit, must be either JSON or a value that can be
     * implicitly converted to JSON.
     * @param feature The name of the feature
     * @param value The value payload of the event
     * @param type The name of the type providing the feature
     * @param instance The name of the instance
     *
     */
    template <typename T>
    void Emit(const std::string& feature, const T& value, const std::string& type = DEFAULT_TYPE,
              const std::string& instance = DEFAULT_INSTANCE) const {
        auto e = OutgoingEvent<T>{subject_, talent_id_, feature, value, type, instance};
        publisher_->Publish(return_topic_, e.Json().dump());
    }

    /**
     * @brief Call the function represented by a Callee. Calling a function and
     * gathering the result is a two step process. First
     * <code>EventContext::Call</code> is called, this generates an pending
     * function call and returns a token. Next the token is passed to
     * <code>EventContext::Gather</code> with a callback function to execute
     * once the result of the call is received.
     *
     * @param callee The callee representing the function to call
     * @param args The arguments to pass to the function
     * @param timeout The maximum time to wait for a reply in ms
     */
    CallToken Call(const Callee& callee, const json& args, const int64_t& timeout = 0) const;

    /**
     * @brief Gather results from pending replies and execute a function. This
     * method is non blocking.
     *
     * @code
     class MyTalent : Talent {
      private:
         Callee myCallee;

      public:
         MyTalent(std::shared_ptr<Publisher> publisher)
             : Talent(MY_TALENT_NAME, publisher) {

             callee0 = CreateCallee(TARGET_TALENT0_NAME, TARGET_FUNCTION0_NAME);
             callee1 = CreateCallee(TARGET_TALENT1_NAME, TARGET_FUNCTION1_NAME);
        }

        void OnEvent(const Event& event, event_ctx_ptr context) override
            if (event.GetType() == MY_DESIRED_TYPE) {
                auto args0 = ...; // Args for the callee0

                // Call function represented by callee0 and store the token
                // associated with the pending reply.
                auto token0= context->Call(callee0, args);

                // Make another call, this time to callee1
                auto args1 = ...;
                auto token1 = context->Call(callee1, args);

                auto handle_reply = [](std::vector<json> replies) {
                    // The order of the replies matches the order in which the
                    // tokens where given to Gather.
                    auto reply = replies[0];

                    log::Info() << "Reply received: " << reply.dump(4);
                    };

                auto handle_timeout = []{
                    log::Info() << "Timed out!";
                };

                // Gather the pending reply, i.e. collect the replies
                // associated with the given tokens and then execute the given
                // callback function.
                context->Gather(handle_reply, handle_timeout, token0, token1);
            }
        }
    }
    * @endcode
    *
    * @param func The function to call when all replies have been gathered
    * @param timeout_func The function to call if any of the pending calls time out or nullptr.
    * @param args Tokens to gather before calling func. The order of the
    * replies passed to func matches the order of the tokens.
    */
    template <typename... Args>
    void Gather(gather_func_ptr func, timeout_func_ptr timeout_func, Args... args) {
        auto now_ms = GetNowMs();
        auto tokens = std::vector<CallToken>{args...};
        auto gatherer = std::make_shared<SinkGatherer>(func, timeout_func, tokens, now_ms);

        reply_handler_->AddGatherer(gatherer);
    }

   protected:
    virtual int64_t GetNowMs() const;

    const std::string talent_id_;
    const std::string channel_id_;
    const std::string subject_;
    const std::string return_topic_;
    reply_handler_ptr reply_handler_;
    publisher_ptr publisher_;
    uuid_generator_func_ptr uuid_gen_;

};

/**
 * @brief CallContext is the context within which a call originated. The
 * purpose of the context is to be able to trace a chain of events and calls.
 */
class CallContext : public EventContext {
   private:
    const std::string feature_;
    const std::string channel_;
    const std::string call_;

   public:
    virtual ~CallContext() = default;

    /**
     * @brief Construct a new CallContext object
     *
     * @param talent_id The id of the Talent for which this context exists
     * @param channel_id The unique channel ID of the Talent
     * @param feature The name of the feature to emit a return value for (if any)
     * @param event The event to base the context of off
     * @param return_topic The name of the topic to reply to
     * @param reply_handler The ReplyHandler to use for collecting replies to outgoing calls
     * @param publisher A publisher to send replies with
     * @param uuid_gen A function generating stringified UUID4s
     */
    CallContext(const std::string& talent_id, const std::string& channel_id, const std::string& feature,
                const Event& event, reply_handler_ptr reply_handler, publisher_ptr publisher, uuid_generator_func_ptr uuid_gen);

    /**
     * @brief Immediately reply to a function call within in this context. Used
     * when a reply can be produced without calling any other external function.
     *
     * ©code
     void Add(const json& args, CallContext context) {
        auto t0 = args[0].get<int>();
        auto t1 = args[1].get<int>();

        context.Reply(t0 + t1);
     }
     * @endcode
     *
     * @param value The result of the function call
     */
    virtual void Reply(const json& value) const;

    /**
     * @brief GatherAndReply replies to a function call received in this
     * context. In contrast to Reply, GatherAndReply is used when the reply
     * depends on further calls whose arguments could not be determined a
     * priori.
     *
     * @code
     void Fibonacci(const json& args, CallContext context) {
         auto n = args[0].get<int>();

         if (n <= 1) {
             context.Reply(n);
             return;
         }

         // Assuming a callee by the name "fib" has been registered
         auto t1 = context.Call(fib, n - 1);
         auto t2 = context.Call(fib, n - 2);

         auto handle_reply = [](std::vector<json> replies) {
             auto n1 = replies[0].get<int>();
             auto n2 = replies[1].get<int>();

             return n1 + n2;
         };

         auto handle_timeout = []{
            log::Info() << "Fibonacci timed out";
        };

         context.GatherAndReply(handle_reply, handle_timeout, t1, t2);
     }
     * @endcode
     *
     * @param func The function to execute once all replies have been gathered
     * @param timeout_func The function to call if any of the pending calls timed out (or nullptr).
     * @param args The tokens to gather before calling func. The order of the
     * replies passed to func matches the order of the tokens.
     */
    template <typename... Args>
    void GatherAndReply(gather_and_reply_func_ptr func, timeout_func_ptr timeout_func, Args... args) {
        auto now_ms = GetNowMs();
        auto tokens = std::vector<CallToken>{args...};
        auto prepared_reply = PreparedFunctionReply{talent_id_, feature_, subject_, channel_, call_, return_topic_, publisher_};
        auto gatherer = std::make_shared<ReplyGatherer>(func, timeout_func, prepared_reply, tokens, now_ms);

        reply_handler_->AddGatherer(gatherer);
    }
};


/**
 * @brief A Talent is the base class for producers and consumers of events.
 *
 */
class Talent {
   public:

   /**
    * @brief Construct a new Talent object
    *
    * @param talent_id Globally (within the system) unique ID of the Talent
    */
    explicit Talent(const std::string& talent_id);

    /**
     * @brief Called periodically in order to fetch the rules describing the
     * set of events that the Talent is interested in. A Talent may change its
     * rule set over time. Override in order to subscribe to events.
     *
     * @code
       schema::rule_ptr OnGetRules() const override {
           // Subscribe to the "temperature" feature of the "device" type that are in the range (3, 10).
           return AndRules{GreaterThan("temperature", 3, "device"),
                           LessThan("temperature", 10, "device")};
       }
     * @endcode
     *
     * @return schema::rule_ptr
     */
    virtual schema::rule_ptr OnGetRules() const;


    /**
     * @brief Called upon reception of an event that matched the Talent's
     * rule set. Override in order to receive events.
     *
     * @code
       void OnEvent(const Event& event, event_ctx_ptr context) override {
          log::Info() << "Received an event:\n" << event.GetValue().dump(4);
       }
     * @endcode
     *
     * @param event Event
     * @param context A pointer to the EventContext in which the event was emitted.
     */
    virtual void OnEvent(const Event& event, event_ctx_ptr context);


    /**
     * @brief Called upon reception of a platform event. Override in order to receive
     * plaform events.
     *
     * @code
       void OnPlatformevent(const PlatformEvent& event) override {
           auto type = event.GetType();

           switch (type) {
               default:
               return;
               case core::PlatformEvent::Type::TALENT_RULES_SET:
                  log::Info() << "Talent " << event.GetData()["talent"].get<std::string>() << " came online";
                  break;
               case core::PlatformEvent::Type::TALENT_RULES_UNSET:
                  log::Info() << "Talent " << event.GetData()["talent"].get<std::string>() << " went offline";
                  break;
           }
      }
     * @endcode
     * @param event PlatformEvent
     */
    virtual void OnPlatformEvent(const PlatformEvent& event);


    /**
     * @brief Called on reception of an error. Override in order to receive
     * error messages.
     *
     * @code
       void OnError(const ErrorMessage& msg) {
          log::Error() << "Something went wrong! Description: " << msg.GetMessage();
       }
     * @endcode
     *
     * @param msg Detailed error message
     */
    virtual void OnError(const ErrorMessage& msg);


    /**
     * @brief Register a function to call from the Talent.
     *
     * @code
       auto callee = RegisterCallee("math_talent", "add");

       ...

       void OnEvent(const Event& event, event_ctx_ptr context) {
          auto term1 = 1;
          auto term2 = 2;

          auto token = context->Call(callee, json{term1, term2});

          auto handle_reply = [](const json& reply) {
             log::Inof() << term1 << " + " << term2 << " = " << reply[0].get<int>();
          }

          auto handle_timeout = nullptr; // no timeout handler

          context->Gather(handle_reply, timeout_handler, token);
       }
     * @endcode
     *
     * @param talent_id The name of the Talent providing the functions
     * @param func The name of the function to call
     * @param type The name of the type (optional)
     *
     * @return Callee
     */
    Callee RegisterCallee(const std::string& talent_id, const std::string& func,
           const std::string& type = "default");


    /////////////////////////////////////////
    //////// Internal methods follow ////////
    /////////////////////////////////////////

    /**
     * @brief Initialize the client (depencency injection).
     *
     * @param reply_handler The ReplyHandler to use for collecting replies to outgoing calls
     * @param context_gen A function generating new EventContext
     * @param uuid_gen A function generating stringified UUID4s
     */
    void Initialize(reply_handler_ptr reply_handler, context_generator_func_ptr context_gen, uuid_generator_func_ptr uuid_gen);

    /**
     * @brief Override the event handler and rule set generation in the Talent.
     * Used for Talents that are run in "callback mode".
     *
     * @param on_event A function to call when the Talent receives an event, substitutes OnEvent
     * @param rules The rule set to send back on discovery
     */
    void SetExternalEventHandler(on_event_func_ptr on_event, schema::rule_ptr rules);

    /**
     * @brief Get the internal rule set. Should not be used by external subclasses.
     *
     * @return schema::rules_ptr
     */
    virtual schema::rules_ptr GetRules() const;

    /**
     * @brief Get the  Schema. The Schema is a concatenation of the internally
     * generated rules and the rules generated by the subclass and informs the
     * plaform of the capabilities of the Talent. Should not be used by
     * external subclasses.
     *
     * @return schema::Schema
     */
    virtual schema::Schema GetSchema() const;

    /**
     * @brief Get the ID of the Talent
     *
     * @return std::string
     */
    std::string GetId() const;

    /**
     * Get the registered callees.
     *
     * @return std::vector<Callee>
     */
    std::vector<Callee> GetCallees();

    /**
     * @brief Get the ID of the Talent's communication channel
     *
     * @return std::string
     */
    std::string GetChannelId() const;

    /**
     * @brief Get the name of an input feature, i.e. "<feature>-in".
     * Should not be used by external subclasses.
     *
     * @param feature The name of the feature
     * @return std::string
     */
    std::string GetInputName(const std::string& feature) const;

    /**
     * @brief Get the name of a talent input feature, i.e. "<talent-id>.<feature>-in".
     * Should not be used by external subclasses.
     *
     * @param talent_id The ID of the talent
     * @param feature The name of the feature
     * @return std::string
     */
    std::string GetInputName(const std::string& talent_id, const std::string& feature) const;

    /**
     * @brief Get the full name of a talent input feature, i.e. "<type>.<talent-id>.<feature>-in".
     * Should not be used by external subclasses.
     *
     * @param type The name of the type
     * @param talent_id The ID of the talent
     * @param feature The name of the feature
     * @return std::string
     */
    std::string GetInputName(const std::string& type, const std::string& talent_id, const std::string& feature) const;

    /**
     * @brief Get the name of an output feature, i.e. "<feature>-out".
     * Should not be used by external subclasses.
     *
     * @param feature The name of the feature
     * @return std::string
     */
    std::string GetOutputName(const std::string& feature) const;

    /**
     * @brief Get the name of a talent output feature, i.e. "<talent-id>.<feature>-out".
     * Should not be used by external subclasses.
     *
     * @param talent_id The ID of the talent
     * @param feature The name of the feature
     * @return std::string
     */
    std::string GetOutputName(const std::string& talent_id, const std::string& feature) const;

    /**
     * @brief Get the full name of a talent output feature, i.e. "<type>.<talent-id>.<feature>-out".
     * Should not be used by external subclasses.
     *
     * @param feature The name of the type
     * @param talent_id The ID of the talent
     * @param feature The name of the feature
     * @return std::string
     */
    std::string GetOutputName(const std::string& type, const std::string& talent_id, const std::string& feature) const;

   protected:
    std::vector<Callee> callees_;
    schema::Talent schema_;
    reply_handler_ptr reply_handler_;

    /**
     * @brief Register a feature provided by the Talent.
     *
     * @param feature The name of the feature
     * @param metadata A description of the feature
     */
    void AddOutput(const std::string& feature, const schema::Metadata& metadata);

    /**
     * @brief Create a new EventContext. Used for emitting the first event or
     * making the first function call in a context. If a new event is emitted
     * as an immediate reaction to having received some other event, then the
     * new event should be emitted using the context of the received event
     * rather than from a new context.
     *
     * @code
       // Emit an event from a new context
       NewEventContext("my-subject")->Emit("my-feature", 42, "my-type");
     * @endcode
     *
     * @param subject Name identifying the context
     * @return event_ctx_ptr
     */
    event_ctx_ptr NewEventContext(const std::string& subject);

   private:
    const std::string talent_id_;
    std::string channel_id_;

    on_event_func_ptr on_event_;
    context_generator_func_ptr context_gen_;
    uuid_generator_func_ptr uuid_gen_;
    schema::rule_ptr rules_ = nullptr;
};

/**
 * @brief A FunctionTalent is Talent that also provides functions.
 *
 */
class FunctionTalent : public Talent {
   private:
    function_map funcs_;

   protected:
    /**
     * @brief Get the internal rule set. Should not be used by external subclasses.
     *
     * @return schema::rules_ptr
     */
    schema::rules_ptr GetRules() const override;

   public:
   /**
    * @brief Construct a new FunctionTalent object.
    *
    * @param talent_id The unique ID of the Talent
    */
    explicit FunctionTalent(const std::string& id);

    /**
     * @brief Register a provided function.
     *
     * @param name The name of the function
     * @param func A callback to invoke when the function is called
     */
    void RegisterFunction(const std::string& name, const func_ptr func);

    /**
     * @brief Skip checking for cyclic references.
     */
    void SkipCycleChecks();


    /////////////////////////////////////////
    //////// Internal methods follow ////////
    /////////////////////////////////////////
    schema::Schema GetSchema() const override;

    /**
     * @brief Get the functions provided functions.
     *
     * @return function_map
     */
    function_map GetFunctions() const;
};


using OnError = std::function<void(const ErrorMessage&)>;
using OnEvent = std::function<void(const Event&, event_ctx_ptr)>;
using OnPlatformEvent = std::function<void(const PlatformEvent&)>;


/**
 * @brief Service wraps a FunctionTalent and provides a convenient interface
 * for providing functions in "callback mode".
 */
class Service {
    public:
    /**
     * @brief Constructs a new Servicea.
     *
     * @param name The name of the service (Talent)
     */
     explicit Service(const std::string& name);

     /**
      * @brief Register a function provided by the Service.
      *
      * @param name The name of the function
      * @param callback The callback to invoke when the function is called
      */
     void RegisterFunction(const std::string& name, func_ptr callback);


    /////////////////////////////////////////
    //////// Internal methods follow ////////
    /////////////////////////////////////////

    /**
     * @brief Get the FunctionTalent represented by the Service.
     *
     * @return std::shared_ptr<FunctionTalent>
     */
     std::shared_ptr<FunctionTalent> GetTalent() const;

    private:
     std::shared_ptr<FunctionTalent> talent_;
};


/**
 * @brief Client handles the interaction between the platform and the Talents.
 */
class Client : public Receiver {

    /**
     * @brief The CalleeTalent is responsible for bridging the gap between
     * subclass and callback mode. In callback mode each stand alone
     * subscription creates a new Talent hidden beneath the surface of the
     * Client. When a stand alone subscription callback is triggered it is
     * possible to issue a function call from it using EventContext::Call().
     * But since function calls require a talent to receive the results and the
     * stand alone subscription by definition doesn't have one, we "secretly"
     * use the CalleTalent as the issuer and receiver of the call by
     * manipulating the EventContext before it is passed to the callback. In
     * fact the CalleTalent is used for all outgoing calls in order simply the
     * code, so even if a call is issued from a fully fledged FunctionTalent
     * the CalleeTalent is still used beneath the surface.
     */
    class CalleeTalent : public Talent {
        public:
         CalleeTalent()
             : Talent(GenerateUUID()) {}

         Callee RegisterCallee(const std::string& talent_id, const std::string& func,
                const std::string& type) {
             auto c = Callee{talent_id, func, type};
             internal_callees_.push_back(c);
             return c;
         }

         bool HasSchema() {
              return !(internal_callees_.empty() && callees_.empty());
         }

         void ClearCallees() {
              callees_.clear();
              callees_.insert(callees_.begin(), internal_callees_.begin(), internal_callees_.end());
         }

         void AddCallees(const std::vector<Callee>& callees) {
             callees_.insert(callees_.begin(), callees.begin(), callees.end());
         }

        private:
         std::vector<Callee> internal_callees_;
    };

    public:

    /**
     * @brief Constructs a new client. An application typically only has one Client.
     *
     * @param connection_string A string describing the broker the Client
     * should connect to, Ex. "tcp://127.0.0.1:1883"
     */
     explicit Client(const std::string& connection_string);

     /**
      * @brief Start the client. All Services, Talents and subscriptions must
      * be created and registerd before Start() is called. When the client is
      * in the "started" state it will attempt to connect to the broker and
      * will try to maintain the connection indefinitely reconnecting if the
      * connection is lost.  This method does not return until Stop() is
      * called.
      */
     void Start();

     /**
      * @brief Stop the client and disconnect from the broker.
      */
     void Stop();

     /**
      * @brief Register a Service. Used in "callback mode".
      *
      * @param service The service.
      */
     void Register(const Service& service);

     /**
      * @brief Register a FunctionTalent. Used in "subclass mode".
      *
      * @param talent The FunctionTalent
      */
     void RegisterFunctionTalent(std::shared_ptr<FunctionTalent> talent);

     /**
      * @brief Register a Talent. Used in "subclass mode".
      *
      * @param talent The Talent
      */
     void RegisterTalent(std::shared_ptr<Talent> talent);

     /**
      * @brief Register a stand alone Callee, i.e. notify the platform that a
      * dependency on a particular function exists. Used in "callback mode".
      *
      * @param talent_id The ID of the Talent providing the function
      * @param func The name of the function
      * @param type The type (optional)
      *
      * @return Callee
      */
     Callee CreateCallee(const std::string& talent_id, const std::string& func, const std::string& type = "default");

     /**
      * @brief Subscribe to a rule set. Used in "callback mode".
      *
      * @param rules The rule set
      * @param callback The callback to invoke when the rules described by the
      * rule set are fulfilled.
      */
     void Subscribe(schema::rule_ptr rules, const OnEvent callback);

     std::function<void(const ErrorMessage& msg)> OnError;
     std::function<void(const PlatformEvent& event)> OnPlatformEvent;

    private:
     /**
      * @brief Parse and distribute a discover message to all Talents.
      *
      * @param msg The raw discover message
      */
     void HandleDiscover(const std::string& msg);

     /**
      * @brief Parse a plaform event and distribute it to all Talents that have
      * registered to receive platform events.
      *
      * @param msg The raw platform message
      */
     void HandlePlatformEvent(const std::string& msg);

     /**
      * @brief Distribute an error message to all Talents that have registered
      * to receive error messages.
      *
      * @param err The error message
      */
     void HandleError(const ErrorMessage& err);

     /**
      * @brief Attempt to treat an event as a function call. A function call is
      * just a special case of event. If an event is sent to a FunctionTalent
      * the feature name must be compared to the registered functions and
      * forwarded to the corresponding function if a match is found.
      *
      * @param talent The FunctionTalent
      * @param event An event
      *
      * @return true if the event was function call
      */
     bool HandleAsCall(std::shared_ptr<FunctionTalent> talent, const Event& event);

     /**
      * @brief Handle an event sent to a particular Talent.
      *
      * @param talent_id The ID of the talent
      * @param msg The raw event
      */
     void HandleEvent(const std::string& talent_id, const std::string& msg);

     /**
      * @brief Handle a reply to a function call.
      *
      * @param talent_id The ID of the talent that made the call
      * @param channel_id The ID of the channel the reply was sent to
      * @param call_id The ID of the call
      * @param msg The raw reply event
      */
     void HandleCallReply(const std::string& talent_id, const std::string&
             channel_id, const call_id_t& call_id, const std::string& msg);

     /**
      * @brief Receive a message from MQTT.
      *
      * @param topic The topic the message was sent one
      * @param msg The message
      */
     void Receive(const std::string& topic, const std::string& msg) override;

     /**
      * @brief Handle the progress of time. This method is called
      * periodically. Used for cleaning out timed out function calls and
      * anything else that needs to be inspected periodically.
      *
      * @param ts A steady clock timestamp, i.e. not wall time
      */
     void UpdateTime(const std::chrono::steady_clock::time_point& ts);

     /**
      * @brief Subscribe to events pertaining to the Client's internal Talents.
      *
      * @param talent The Talent
      */
     void SubscribeInternal(std::shared_ptr<Talent> talent);

     /**
      * @brief Get the discover topic.
      *
      * @return std::string
      */
     std::string GetDiscoverTopic() const;

     /**
      * @brief Get the topic's shared prefix.
      *
      * @return std::string
      */
     std::string GetSharedPrefix(const std::string& talent_id) const;

     /**
      * @brief Get the event topic.
      *
      * @return std::string
      */
     std::string GetEventTopic(const std::string& talent_id) const;

     /**
      * @brief Get the platform events topic.
      *
      * @return std:.string
      */
     std::string GetPlatformEventsTopic() const;

     std::shared_ptr<MqttClient> mqtt_client_;
     std::shared_ptr<CalleeTalent> callee_talent_;
     std::unordered_map<std::string, std::shared_ptr<FunctionTalent>> function_talents_;
     std::unordered_map<std::string, std::shared_ptr<Talent>> subscription_talents_;
     reply_handler_ptr reply_handler_;
     const std::string mqtt_topic_ns_;
};

}  // namespace core
}  // namespace iotea

#endif // IOTEA_IOTEA_HPP
