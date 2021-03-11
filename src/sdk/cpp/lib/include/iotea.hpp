/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#ifndef IOTEA_HPP
#define IOTEA_HPP

#include <chrono>
#include <initializer_list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "nlohmann/json.hpp"

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
class EventContext;
class CallContext;
class Callee;
class CallHandler;
class DiscoverMessage;

using func_ptr = std::function<void(const json&, const CallContext&)>;
using function_map = std::unordered_map<std::string, func_ptr>;
using gather_func_ptr = std::function<void(std::vector<std::pair<json, EventContext>>)>;
using gather_and_reply_func_ptr = std::function<json(std::vector<std::pair<json, EventContext>>)>;
using message_ptr = std::shared_ptr<Message>;
using talent_ptr = std::shared_ptr<Talent>;
using talent_map = std::unordered_map<std::string, talent_ptr>;
using publisher_ptr = std::shared_ptr<Publisher>;
using call_handler_ptr = std::shared_ptr<CallHandler>;

using duration_t = std::chrono::duration<int64_t>;
using timepoint_t = std::chrono::system_clock::time_point;

using call_token_t = std::string;

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
        EVENT = 1,
        DISCOVER = 2,
        UNKNOWN_ATM = 3,  // TODO what this means and whether or not it is even defined is unknown at the moment
        ERROR = 4
    };

   protected:
    enum Type msg_type_ = Type::EVENT;
    int code_;

   public:
    /**
     * @brief Construct a new Message object.
     *
     * @param msg_type Type of message
     * @param code Error code, only relevant if the type is Message::Type::ERROR
     */
    explicit Message(const Type msg_type, const int code = 0);

    /**
     * @brief Test if this is an event message.
     *
     * @return true if this is an event message
     * @return false if this is not an event message
     */
    bool IsEvent() const;

    /**
     * @brief Test if this is a discover message.
     *
     * @return true if this is a discover message
     * @return false if this is not a discover message
     */
    bool IsDiscover() const;

    /**
     * @brief Test if this is an error message.
     *
     * @return true if this is an error message
     * @return false if this is not an error message
     */
    bool IsError() const;

    /**
     * @brief Get the code associated with this message.
     *
     * @return int
     */
    int GetCode() const;

    /**
     * @brief Create a Message from JSON.
     *
     * @param j JSON prepresentation of a Message.
     * @return Message
     */
    static Message FromJson(const json& j);
};

/**
 * @brief DiscoverMessage is periodically sent by the platform in order to
 * trigger the Talent to reply with a schema detailing what the Talent produces
 * and what it consumes. When a DiscoveryMessage is received
 * Talent::OnGetRules() is called in order to fetch Talent specific rules.
 * Should not be created by external clients.
 *
 */
class DiscoverMessage {
   private:
    const std::string version_;
    const std::string return_topic_;

   public:
    /**
     * @brief Construct a new DiscoverMessage object
     *
     * @param version Version of the message
     * @param return_topic Name of topic to reply with schema to.
     */
    DiscoverMessage(const std::string& version, const std::string& return_topic);

    /**
     * @brief Get the DiscoverMessage version
     *
     * @return std::string
     */
    std::string GetVersion() const;

    /**
     * @brief Get the name of the return topic.
     *
     * @return std::string
     */
    std::string GetReturnTopic() const;

    /**
     * @brief Create a DiscoverMessage from JSON.
     *
     * @param j JSON representation of a DiscoverMessage
     * @return DiscoverMessage
     */
    static DiscoverMessage FromJson(const json& j);
};

class PlatformEvent {
   public:
    enum class Type {
        TALENT_RULES_SET,
        TALENT_RULES_UNSET,
        UNDEF // TODO extend
    };

   private:
    const Type type_;
    const json data_;
    const timepoint_t timestamp_;

   public:
   PlatformEvent(const Type& type, const json& data, const timepoint_t& timestamp);

   json GetData() const;

   timepoint_t GetTimestamp() const;

   Type GetType() const;

   static PlatformEvent FromJson(const json& j);
};

/**
 * @brief ErrorMessage is returned by the platform when something has gone
 * wrong. Should not be created by external clients.
 *
 */
class ErrorMessage {
   private:
    const int code_;

   public:
    /**
     * @brief Construct a new ErrorMessage object
     *
     * @param code Numerical error code
     */
    explicit ErrorMessage(const int code);

    /**
     * @brief Get a description of the error.
     *
     * @return std::string
     */
    std::string GetMessage() const;

    /**
     * @brief Get the Code
     *
     * @return int
     */
    int GetCode() const;

    /**
     * @brief Create an ErrorMessage from JSON.
     *
     * @param j JSON representation of an ErrorMessage
     * @return ErrorMessage
     */
    static ErrorMessage FromJson(const json& j);
};

/**
 * @brief Event represents an incoming event. Should not be used by external clients.
 *
 */
class Event {
   private:
    std::string return_topic_;
    std::string subject_;
    std::string feature_;
    json value_;
    std::string type_;
    std::string instance_;
    timepoint_t when_;

   public:
    /**
     * @brief Construct a new Event object
     *
     * @param subject Name of subject as determined by the context from which the event originated
     * @param feature Name of the feature that emitted the event
     * @param value Payload value
     * @param type Name of the type associated with the event
     * @param instance Name of the instance associated with the event
     * @param return_topic Name of topic on which to issue replies if any
     * @param when Point in time when the event was emitted on the emitter's side
     */
    Event(const std::string& subject, const std::string& feature, const json& value,
          const std::string& type = "default", const std::string& instance = "default",
          const std::string& return_topic = "", const timepoint_t& when = std::chrono::system_clock::now());

    /**
     * @brief Get the return topic name.
     *
     * @return std::string
     */
    std::string GetReturnTopic() const;

    /**
     * @brief Get the subject name.
     *
     * @return std::string
     */
    std::string GetSubject() const;

    /**
     * @brief Get the feature name.
     *
     * @return std::string
     */
    std::string GetFeature() const;

    /**
     * @brief Get the payload value as JSON.
     *
     * @return json
     */
    json GetValue() const;

    /**
     * @brief Get the name of the instance.
     *
     * @return std::string
     */
    std::string GetInstance() const;

    /**
     * @brief Get the name of the type.
     *
     * @return std::string
     */
    std::string GetType() const;

    /**
     * @brief Get the time when the event was emitted.
     *
     * @return timepoint_t
     */
    timepoint_t GetWhen() const;

    /**
     * @brief Get a representation of the event as JSON.
     *
     * @return json
     */
    json Json() const;

    /**
     * @brief Create an event from JSON.
     *
     * @param j JSON representaion of an event
     * @return Event
     */
    static Event FromJson(const json& j);
};

/**
 * @brief OutgoingEvent contains all the necessary information required to emit
 * an event. Should not be used by external clients.
 *
 * @tparam T Type of the event payload value
 */
template <typename T>
class OutgoingEvent {
   private:
    const std::string subject_;
    const std::string feature_;
    const T value_;
    const std::string type_;
    const std::string instance_;
    const timepoint_t when_;

   public:
    /**
     * @brief Construct an OutgoingEvent object
     *
     * @param subject Name of subject as determined by the context from which the event is emitted
     * @param feature Name of feature producing the event
     * @param value Payload value
     * @param type Name of the type associated with the event
     * @param instance Name of the instance producing the event
     */
    OutgoingEvent(const std::string& subject, const std::string& feature, const T& value, const std::string& type,
                  const std::string& instance)
        : subject_{subject}
        , feature_{feature}
        , value_{value}
        , type_{type}
        , instance_{instance}
        , when_{std::chrono::system_clock::now()} {}

    /**
     * @brief Get a JSON representation of this OutgoingEvent
     *
     * @return json
     */
    json Json() const {
        auto duration = when_.time_since_epoch();
        auto when_ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

        return json{{"subject", subject_}, {"feature", feature_},   {"value", value_},
                    {"type", type_},       {"instance", instance_}, {"whenMs", when_ms}};
    }
};

/**
 * @brief OutgoingCall contains all the necessary information required to
 * perform a function call. Should not be used by external clients.
 *
 */
class OutgoingCall {
   private:
    const std::string talent_id_;
    const std::string channel_id_;
    const std::string call_id_;
    const std::string func_;
    const json& args_;
    const std::string subject_;
    const std::string type_;

   public:
   /**
    * @brief Construct a new OutgoingCall object
    *
    * @param talent_id ID of the Talent providing the function
    * @param channel_id ID of the channel determined by the context from which the call is made
    * @param call_id Unique ID of the call provided by the caller
    * @param func Name of the function to call
    * @param args Arguments to pass to the function
    * @param subject Name of subject as determined by the context from which the call is made
    * @param type Name of the type associated with the called function
    */
    OutgoingCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                 const std::string& func, const json& args, const std::string& subject, const std::string& type);

    /**
     * @brief Get a JSON representation of this OutgoingCall
     *
     * @return json
     */
    json Json() const;

    /**
     * @brief Get the Call ID.
     *
     * @return std::string
     */
    std::string GetCallId() const;
};

/**
 * @brief The Publisher interface describes the methods required to emit events
 * and make function calls.
 *
 */
class Publisher {
   public:
    virtual void Publish(const std::string& topic, const std::string& data) = 0;
    virtual std::string GetIngestionEventsTopic() const = 0;
    virtual std::string GetNamespace() const = 0;
};

/**
 * @brief A Callee represents a callable function provided by some Talent. It
 * is responsible for managing calls to and replies from the function as well
 * as for holding information about the function call details necessary to
 * properly generate the Talent schema. A Callee should not be created directly
 * but through Talent::GetCallee().
 *
 */
class Callee {
   private:
    std::string talent_id_;
    std::string func_;
    std::string type_;
    publisher_ptr publisher_;

    bool registered_;

   public:
   /**
    * @brief Construct a new Callee.
    *
    */
    Callee();

    /**
     * @brief Construct a new Callee. Clients should not call this constructor
     * directly but should use Talent::CreateCallee().
     *
     * @param talent_id ID of the Talent providing the function
     * @param func Name of the function
     * @param type Name of the type associated with the function
     * @param publisher // TODO document
     */
    Callee(const std::string& talent_id, const std::string& func, const std::string& type, publisher_ptr publisher);

    /**
     * @brief Call the function represented by the Callee, results will be
     * routed to the provided callback.
     *
     * @param args Arguments to pass to the function
     * @param ctx Context associated with the call
     */
    call_token_t Call(const json& args, const EventContext& ctx) const;

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
};

/**
 * @brief EventContext is the context within which a set of events and calls
 * originate. The purpose of the context is to be able to trace a chain of
 * events and calls.
 *
 */
class EventContext {
   protected:
    const std::string talent_id_;
    const std::string channel_id_;
    const std::string subject_;
    const std::string return_topic_;
    call_handler_ptr call_handler_;
    publisher_ptr publisher_;

   public:
   /**
    * @brief Construct a new EventContext. External clients should not call
    * this constructor explicitly but should instead use
    * Talent.NewEventContext().
    *
    * @param talent Talent associated with the context
    * @param publisher Publisher for sending data
    * @param subject Name identifying the context
    * @param return_topic The topic to reply to function call ons
    */
    EventContext(const std::string& talent_id, const std::string& channel_id, const std::string& subject,
                 const std::string& return_topic, call_handler_ptr call_handler, publisher_ptr publisher);

    /**
     * @brief Construct an EventContext object based on the context of an
     * Event. Should not be used by external clients.
     *
     * @param talent Talent associated with the context
     * @param publisher Publisher for sending data
     * @param event Event to base context on
     */
    EventContext(const std::string& talent_id, const std::string& channel_id, const Event& event,
                 call_handler_ptr call_handler, publisher_ptr publisher);

    std::string GetTalentId() const;

    std::string GetChannelId() const;

    std::string GetSubject() const;

    std::string GetReturnTopic() const;

    /**
     * @brief Emit an event within this context.
     *
     * @code
       context.Emit<int>("temperature", degrees, "device");
     * @endcode
     *
     * @tparam T Type of the event to emit
     * @param feature Name of the feature
     * @param value Value payload of the event
     * @param type Name of the type providing the feature
     *
     */
    template <typename T>
    void Emit(const std::string& feature, const T& value, const std::string& type = "default") const {
        auto e = OutgoingEvent<T>{subject_, feature, value, type, talent_id_};

        publisher_->Publish(return_topic_, e.Json().dump());
    }

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

             myCallee = CreateCallee(TARGET_TALENT_NAME, TARGET_FUNCTION_NAME);
        }

        void OnEvent(const Event& event, EventContext context) override
            if (event.GetType() == MY_DESIRED_TYPE) {
                auto args = ...; // Args for myCallee

                // Call function represented by myCallee and store the token
                // associated with the pending reply.
                auto token = myCallee.Call(args, context);

                // Gather the pending reply, i.e. collect the replies
                // associated with the given tokens and then execute the given
                // callback function.
                context.Gather([]{std::vector<std::pair<json, EventContext>> replies) {
                    // The replies vector holds the replies as pairs of
                    // * first:  the actual JSON reply value
                    // * second: the EventContext from which the call was made
                    //
                    // The order of the replies matches the order in which the
                    // tokens where given to Gather.
                    auto reply = replies[0].first;

                    log::Info() << "Reply received: " << reply.dump(4);
                }, {token});
            }
        }
    }
    * @endcode
    *
    * @param func Callback function to call when all replies have been gathered
    * @param tokens std::vector<call_token_t> of tokens to gather before
    * calling func. The order of the replies passed to func matches the order
    * of the tokens.
    */
    void Gather(gather_func_ptr func, const std::vector<call_token_t>& tokens);
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
   /**
    * @brief Construct a new CallContext object
    *
    * @param talent Talent associated with the context
    * @param publisher Publisher for sending data
    * @param feature Name of the feature
    * @param event Event for which the context applies
    */
    CallContext(const std::string& talent_id, const std::string& channel_id, const std::string& feature,
                const Event& event, call_handler_ptr call_handler, publisher_ptr publisher);

    /**
     * @brief Reply replies to a function call received in this context.
     *
     * @param value Result of function call
     */
    void Reply(const json& value) const;

    /**
     * @brief GatherAndReply replies to a function call received in this
     * context. In contrast to Reply, GatherAndReply should be used when the
     * reply depends on further calls whose arguments could not be determined a
     * priori.
     *
     * @code
     void Fibonacci(const json& args, CallContext context) {
         auto n = args[0].get<int>();

         if (n <= 1) {
             context.Reply(n);
             return;
         }

         auto t1 = fib.Call(n - 1, context);
         auto t2 = fib.Call(n - 2, context);

         context.GatherAndReply([](std::vector<std::pair<json, EventContext>> replies) {
                 auto n1 = replies[0].first.get<int>();
                 auto n2 = replies[1].first.get<int>();

                 return n1 + n2;
                 }, {t1, t2});
     }
     * @endcode
     *
     * @param func Callback to execute once all replies have been gathered
     * @param tokens Pending replies on which func dependes
     */
    void GatherAndReply(gather_and_reply_func_ptr func, const std::vector<call_token_t>& tokens);
};


/**
 * @brief ReplyGatherer associates a set of pending replies with a callback
 * function. When the all the replies have been received the ReplyGatherer is
 * done and the replies can be forwarded to the callback.
 *
 * @tparam Func Function signature associated with the replies
 * @tparam Ctx Context type from which the callback should be executed
 */
template <typename Func, typename Ctx>
class ReplyGatherer {
   private:
    const Func func_;
    const Ctx ctx_;
    const std::vector<call_token_t> order_;

    std::unordered_map<call_token_t, std::pair<json, EventContext>> replies_;

   public:
    /**
     * @brief Construct a new ReplyGatherer object
     *
     * @param func callback function
     * @param ctx callback context
     * @param tokens ordered list of tokens representing pending replies
     */
    ReplyGatherer(Func func, const Ctx& ctx, std::vector<call_token_t> tokens)
        : func_{func}
        , ctx_{ctx}
        , order_{tokens} {}

    /**
     * @brief Gather accepts the reply associated with token and stores it
     * until it can be forwarded to the callback.
     *
     * @param token token associated with the pending reply
     * @param reply pair of function reply and the context from which it emanated
     */
    void Gather(const call_token_t& token, const std::pair<json, EventContext>& reply) {
        if (std::find(order_.begin(), order_.end(), token) == order_.end()) {
            log::Error() << "Unrecognized call id " << token;
            return;
        }

        replies_.insert(std::make_pair(token, reply));
    }

    /**
     * @brief IsDone returns whether all the pending replies have been gathered.
     *
     * @return true iff. all replies have been gathered.
     */
    bool IsDone() const {
        return order_.size() == replies_.size();
    }

    /**
     * @brief GetReplies returns all the gathered tokens and their associated
     * replies. This function must not be called before IsDone returns true.
     *
     * @return std::unordered_map<call_token_t, std::pair<json, EventContext>>
     */
    std::unordered_map<call_token_t, std::pair<json, EventContext>> GetReplies() const {
        return replies_;
    }

    /**
     * @brief GetFunc returns the callback function associated with this ReplyGatherer.
     *
     * @return Func
     */
    Func GetFunc() const {
        return func_;
    }

    /**
     * @brief GetFunc returns the context associated with this ReplyGatherer.
     *
     * @return Ctx
     */
    Ctx GetContext() const {
        return ctx_;
    }
};

/**
 * @brief CallHandler maintains a registry of pending function calls
 * and their associated result handling functions. CallHandler should not be
 * used by external clients.
 *
 */
class CallHandler {
   private:
    std::unordered_map<call_token_t, std::shared_ptr<ReplyGatherer<gather_func_ptr, EventContext>>> gatherer_map_;
    std::unordered_map<call_token_t, std::shared_ptr<ReplyGatherer<gather_and_reply_func_ptr, CallContext>>> gatherer_and_reply_map_;

    void HandleGather(std::shared_ptr<ReplyGatherer<gather_func_ptr, EventContext>> gatherer, const call_token_t& token, const std::pair<json, EventContext>& reply);
    void HandleGatherAndReply(std::shared_ptr<ReplyGatherer<gather_and_reply_func_ptr, CallContext>> gatherer, const call_token_t& token, const std::pair<json, EventContext>& reply);

   public:
    /**
     * @brief Gather stores a callback function to be called when the pending
     * replies associated with tokens have been gathered.
     *
     * @param func Callback function
     * @param tokens Call tokens
     */
    void Gather(gather_func_ptr func, const EventContext& ctx, std::vector<call_token_t> tokens);

    /**
     * @brief GatherAndReply stores a callback function to be called when the
     * pending replies associated with tokens have been gathered.
     *
     * @param func Callback function
     * @param tokens Call tokens
     */
    void GatherAndReply(gather_and_reply_func_ptr func, const CallContext& ctx, std::vector<call_token_t> tokens);

    /**
     * @brief HandleReply forwards a reply to the proper ReplyGatherer. If the
     * ReplyGatherer has gathered all the required replies (IsDone) the
     * assicated callback is called and the ReplyGatherer is freed.
     *
     * @param token Call token.
     * @param reply Reply associated with token.
     */
    void HandleReply(const call_token_t& token, const std::pair<json, EventContext>& reply);
};

/**
 * @brief A Talent is the base class for producers and consumers of events.
 *
 */
class Talent {
   private:
    const std::string talent_id_;
    const std::string channel_id_;
    std::set<std::string> callees_;

   protected:
    publisher_ptr publisher_;
    schema::Talent schema_;
    call_handler_ptr call_handler_;

    /**
     * @brief Get the Publisher associated with the Talent. Should not be used
     * by external subclasses.
     *
     * @return publisher_ptr
     */
    publisher_ptr GetPublisher() const;

    /**
     * @brief Register a feature provided by the Talent.
     *
     * @param feature Name of the feature
     * @param metadata Description of the feature
     */
    void AddOutput(const std::string& feature, const schema::Metadata& metadata);

    /**
     * @brief Get the this Talent's Schema. The Schema is a concatenation of
     * the internally generated rules and the rules generated by the subclass
     * and informs the plaform of the capabilities of the Talent. Should not be
     * used by external subclasses.
     *
     * @return schema::Schema
     */
    schema::Schema GetSchema() const;

    /**
     * @brief Create a new EventContext. Used for emitting the first event in a
     * context. If an event is emitted as the result of receiving some other
     * event the context of the other event should be used rather than creating
     * a new one.
     *
     * @code
       // Emit an event from a new context
       NewEventContext("my-subject").Emit<int>("my-feature", 42, "my-type");
     * @endcode
     *
     * @param subject Name identifying the context
     * @return EventContext
     */
    EventContext NewEventContext(const std::string& subject);

    /**
     * @brief Get the internal rule set. Should not be used by external subclasses.
     *
     * @return schema::rules_ptr
     */
    virtual schema::rules_ptr GetRules() const;

    /**
     * @brief Called on error.
     *
     * @param msg Detailed error message
     */
    virtual void OnError(const ErrorMessage& msg);

    /**
     * @brief Called upon reception of an event that matched the Talent's
     * ruleset.
     *
     * @param event Event
     * @param context Context in which the event was emitted.
     */
    virtual void OnEvent(const Event& event, EventContext context);

    /**
     * @brief Call upon reception of a platform event.
     *
     * @param event PlatformEvent
     */
    virtual void OnPlatformEvent(const PlatformEvent& event);

    /**
     * @brief Called periodically in order to fetch the rules describing the
     * set of events the Talent is interested in. The Talent may change is
     * ruleset over time.
     *
     * @code
       schema::rules_ptr OnGetRules() const override {
           // Subscribe to the "temperature" feature of the "device" type that are in the range (3, 10).
           return OrRules({AndRules({
               GreaterThan("temperature", 3, "device"),
               LessThan("temperature", 10, "device")
           }));
       }
     * @endcode
     *
     * @return schema::rules_ptr
     */
    virtual schema::rules_ptr OnGetRules() const;

   public:
   /**
    * @brief Construct a new Talent object
    *
    * @param talent_id Globally (within the system) unique ID of the Talent
    * @param publisher Publisher for sending data
    */
    Talent(const std::string& talent_id, publisher_ptr publisher);

    /**
     * @brief Get the ID of the Talent
     *
     * @return std::string
     */
    std::string GetId() const;

    /**
     * @brief Create a Callee
     *
     * @param talent_id ID of the Talent providing the callable
     * @param func Name of the callable function
     * @param type Name of the type associated with the callable function
     * @return Callee
     */
    Callee CreateCallee(const std::string& talent_id, const std::string& func,
           const std::string& type = "default");

    /**
     * @brief Get the name of the Talent's communication channel
     *
     * @return std::string
     */
    std::string GetChannelId() const;

    /**
     * @brief Handle incoming event by unmarshalling and forwarding it to the
     * appropriate callback. Should not be used by external subclasses.
     *
     * @param data Raw event payload data
     */
    void HandleEvent(const std::string& data);

    /**
     * @brief Handle incoming event by and forward it to OnEvent with the
     * associated context. Should not be used by external subclasses
     *
     * @param data Event payload data
     */
    virtual void HandleEvent(const Event& event);

    /**
     * @brief Handle incoming discovery request by generating and emitting an
     * event with the Talent capability schema. Should not be used by external
     * subsclasses.
     *
     * @param data Payload
     */
    void HandleDiscover(const std::string& data);

    /**
     * @brief Handle incoming platorm event by forwading it ot OnPlatformEvent
     * with the associated payload.
     *
     * @param data Payload
     */
    void HandlePlatformEvent(const std::string& data);

    /**
     * @brief Handle replies to function calls by unmarshalling the result and
     * forwarding it and the associated context to the appropriate callback.
     * Not to be used by external subsclasses.
     *
     * @param channel_id Channel the call was made on
     * @param call_id Unique ID of the call to be paired with the ID generated
     * when the call was issued.
     * @param data Raw result data
     */
    void HandleReply(const std::string& channel_id, const std::string& call_id, const std::string& data);

    /**
     * @brief Get the CallHandler for this Talent. Should not be used by
     * external subclasses.
     *
     * @return call_handler_ptr
     */
    call_handler_ptr GetCallHandler() const;
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
    * @param talent_id Globally (within the system) unique ID of the Talent
    * @param publisher Publisher for sending data
    */
    FunctionTalent(const std::string& id, publisher_ptr publisher);

    /**
     * @brief Handle incoming event by and forward it to OnEvent with the
     * associated context. Should not be used by external subclasses
     *
     * @param data Event payload data
     */
    void HandleEvent(const Event& event) override;

    /**
     * @brief Register a function provided by the FunctionTalent.
     *
     * @param name Name of the function
     * @param func Callback to invoke when function is called
     */
    void RegisterFunction(const std::string& name, const func_ptr func);

    /**
     * @brief Toggle whether the platform should accept cyclic references
     * between functions or not.
     *
     * @param skip Skip cycle checks iff. @code skip @endcode is @code true
     * @endcode
     */
    void SkipCycleCheck(bool skip);

    /**
     * @brief Get the full name of an input, i.e. "<talent-id>.<feature>-in".
     * Should not be used by external subclasses.
     *
     * @param feature Name of feature
     * @return std::string
     */
    std::string GetInputName(const std::string& feature) const;

    /**
     * @brief Get the full name of an output, i.e. "<talent-id>.<feature>-out".
     * Should not be used by external subclasses.
     *
     * @param feature Name of feature
     * @return std::string
     */
    std::string GetOutputName(const std::string& feature) const;
};

}  // namespace core
}  // namespace iotea

#endif
