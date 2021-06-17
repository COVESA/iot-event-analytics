/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_TALENT_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_TALENT_HPP_

#include <functional>
#include <string>
#include <vector>

#include "event.hpp"
#include "call.hpp"
#include "context.hpp"

using iotea::core::logging::Logger;
using iotea::core::logging::NamedLogger;

namespace iotea {
namespace core {

using func_ptr = std::function<void(const json&, call_ctx_ptr)>;
using context_generator_func_ptr = std::function<event_ctx_ptr(const std::string& subject)>;
using on_event_func_ptr = std::function<void(event_ptr, event_ctx_ptr)>;

using function_map = std::unordered_map<std::string, func_ptr>;

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

    virtual ~Talent() = default;

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
       void OnEvent(event_ptr event, event_ctx_ptr context) override {
          log::Info() << "Received an event:\n" << event->GetValue().dump(4);
       }
     * @endcode
     *
     * @param event A pointer to the event Event
     * @param context A pointer to the EventContext in which the event was emitted.
     */
    virtual void OnEvent(event_ptr event, event_ctx_ptr context);

    /**
     * @brief Called upon reception of a platform event. Override in order to receive
     * plaform events.
     *
     * @code
       void OnPlatformevent(platform_event_ptr event) override {
           auto type = event->GetType();

           switch (type) {
               default:
               return;
               case core::PlatformEvent::Type::TALENT_RULES_SET:
                  log::Info() << "Talent " << event->GetData()["talent"].get<std::string>() << " came online";
                  break;
               case core::PlatformEvent::Type::TALENT_RULES_UNSET:
                  log::Info() << "Talent " << event->GetData()["talent"].get<std::string>() << " went offline";
                  break;
           }
      }
     * @endcode
     * @param event A pointer to a PlatformEvent
     */
    virtual void OnPlatformEvent(platform_event_ptr event);

    /**
     * @brief Called on reception of an error. Override in order to receive
     * error messages.
     *
     * @code
       void OnError(error_message_ptr msg) {
          log::Error() << "Something went wrong! Description: " << msg->GetMessage();
       }
     * @endcode
     *
     * @param msg Detailed error message
     */
    virtual void OnError(error_message_ptr msg);

    /**
     * @brief Register a function to call from the Talent.
     *
     * @code
       auto callee = RegisterCallee("math_talent", "add");

       ...

       void OnEvent(event_ptr, event_ctx_ptr context) {
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
    virtual Callee RegisterCallee(const std::string& talent_id, const std::string& func,
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
    virtual void Initialize(reply_handler_ptr reply_handler, context_generator_func_ptr context_gen, uuid_generator_func_ptr uuid_gen);

    /**
     * @brief Override the event handler and rule set generation in the Talent.
     * Used for Talents that are run in "callback mode".
     *
     * @param on_event A function to call when the Talent receives an event, substitutes OnEvent
     * @param rules The rule set to send back on discovery
     */
    virtual void SetExternalEventHandler(on_event_func_ptr on_event, schema::rule_ptr rules);

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
    virtual std::string GetId() const;

    /**
     * Get the registered callees.
     *
     * @return std::vector<Callee>
     */
    virtual std::vector<Callee> GetCallees();

    /**
     * @brief Get the ID of the Talent's communication channel
     *
     * @return std::string
     */
    virtual std::string GetChannelId() const;

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

    /**
     * @brief Get the logger associated with this Talent.
     *
     * @return NamedLogger
     */
    NamedLogger GetLogger() const;

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
    virtual void AddOutput(const std::string& feature, const schema::Metadata& metadata);

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
    virtual event_ctx_ptr NewEventContext(const std::string& subject);

   private:
    const std::string talent_id_;
    std::string channel_id_;
    NamedLogger logger_;

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


}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_TALENT_HPP_
