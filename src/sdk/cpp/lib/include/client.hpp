/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_CLIENT_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_CLIENT_HPP_

#include <memory>
#include <vector>
#include <unordered_map>
#include <string>

#include "mqtt_client.hpp"
#include "event.hpp"
#include "call.hpp"
#include "talent.hpp"


namespace iotea {
namespace core {


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
     explicit CalleeTalent(const std::string& id);

     virtual ~CalleeTalent() = default;

     virtual Callee RegisterCallee(const std::string& talent_id, const std::string& func,
            const std::string& type);

     virtual bool HasSchema() const;
     virtual void ClearCallees();
     virtual void AddCallees(const std::vector<Callee>& callees);

    private:
     std::vector<Callee> internal_callees_;
};


/**
 * @brief Client handles the interaction between the platform and the Talents.
 */
class Client : public Receiver {
    public:
    /**
     * @brief Constructs a new client. An application typically only has one Client.
     *
     * @param connection_string A string describing the broker the Client
     * should connect to, Ex. "tcp://127.0.0.1:1883"
     */
     explicit Client(const std::string& connection_string);

     virtual ~Client() = default;

     /**
      * @brief Start the client. All Services, Talents and subscriptions must
      * be created and registerd before Start() is called. When the client is
      * in the "started" state it will attempt to connect to the broker and
      * will try to maintain the connection indefinitely reconnecting if the
      * connection is lost.  This method does not return until Stop() is
      * called.
      */
     virtual void Start();

     /**
      * @brief Stop the client and disconnect from the broker.
      */
     virtual void Stop();

     /**
      * @brief Register a Service. Used in "callback mode".
      *
      * @param service The service.
      */
     virtual void Register(const Service& service);

     /**
      * @brief Register a FunctionTalent. Used in "subclass mode".
      *
      * @param talent The FunctionTalent
      */
     virtual void RegisterFunctionTalent(std::shared_ptr<FunctionTalent> talent);

     /**
      * @brief Register a Talent. Used in "subclass mode".
      *
      * @param talent The Talent
      */
     virtual void RegisterTalent(std::shared_ptr<Talent> talent);

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
     virtual Callee CreateCallee(const std::string& talent_id, const std::string& func, const std::string& type = "default");

     /**
      * @brief Subscribe to a rule set. Used in "callback mode".
      *
      * @param rules The rule set
      * @param callback The callback to invoke when the rules described by the
      * rule set are fulfilled.
      */
     virtual void Subscribe(schema::rule_ptr rules, const OnEvent callback);

     std::function<void(const ErrorMessage& msg)> OnError;
     std::function<void(const PlatformEvent& event)> OnPlatformEvent;

    protected:
        Client(std::shared_ptr<MqttClient> mqtt_client, std::shared_ptr<CalleeTalent> callee_talent, reply_handler_ptr reply_handler, const std::string& mqtt_topic_ns);

     /**
      * @brief Parse and distribute a discover message to all Talents.
      *
      * @param msg The raw discover message
      */
     virtual void HandleDiscover(const std::string& msg);

     /**
      * @brief Parse a plaform event and distribute it to all Talents that have
      * registered to receive platform events.
      *
      * @param msg The raw platform message
      */
     virtual void HandlePlatformEvent(const std::string& msg);

     /**
      * @brief Distribute an error message to all Talents that have registered
      * to receive error messages.
      *
      * @param err The error message
      */
     virtual void HandleError(const ErrorMessage& err);

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
     virtual bool HandleAsCall(std::shared_ptr<FunctionTalent> talent, const Event& event);

     /**
      * @brief Handle an event sent to a particular Talent.
      *
      * @param talent_id The ID of the talent
      * @param msg The raw event
      */
     virtual void HandleEvent(const std::string& talent_id, const std::string& msg);

     /**
      * @brief Handle a reply to a function call.
      *
      * @param talent_id The ID of the talent that made the call
      * @param channel_id The ID of the channel the reply was sent to
      * @param call_id The ID of the call
      * @param msg The raw reply event
      */
     virtual void HandleCallReply(const std::string& talent_id, const std::string&
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
      * @param ts The current epoch time in ms.
      */
     virtual void UpdateTime(int64_t ts);

     /**
      * @brief Subscribe to events pertaining to the Client's internal Talents.
      *
      * @param talent The Talent
      */
     virtual void SubscribeInternal(std::shared_ptr<Talent> talent);

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

    private:
     std::shared_ptr<MqttClient> mqtt_client_;
     std::shared_ptr<CalleeTalent> callee_talent_;
     std::unordered_map<std::string, std::shared_ptr<FunctionTalent>> function_talents_;
     std::unordered_map<std::string, std::shared_ptr<Talent>> subscription_talents_;
     reply_handler_ptr reply_handler_;
     const std::string mqtt_topic_ns_;
};


}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_CLIENT_HPP_

