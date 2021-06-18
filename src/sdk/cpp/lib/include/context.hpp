
/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_CONTEXT_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_CONTEXT_HPP_

#include <string>

#include "call.hpp"
#include "event.hpp"
#include "protocol_gateway.hpp"

namespace iotea {
namespace core {

using reply_handler_ptr = std::shared_ptr<ReplyHandler>;

/**
 * @brief EventContext is the context within which an event exists.
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
     * @param gateway A gateway to send replies with
     * @param uuid_gen A function generating stringified UUID4s
     */
    EventContext(const std::string& talent_id, const std::string& channel_id, const std::string& subject,
                 const std::string& return_topic, reply_handler_ptr reply_handler, gateway_ptr gateway, uuid_generator_func_ptr uuid_gen);

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
     */
    template <typename T>
    void Emit(const std::string& feature, const T& value, const std::string& type = DEFAULT_TYPE,
              const std::string& instance = DEFAULT_INSTANCE) const {
        auto e = OutgoingEvent<T>{subject_, talent_id_, feature, value, type, instance};
        gateway_->Publish(return_topic_, e.Json().dump());
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
     * @return CallToken
     *
     * @throw std::logic_error If timeout <= 0
     */
    virtual CallToken Call(const Callee& callee, const json& args, int64_t timeout = 10000) const;

    /**
     * @brief Gather results from pending replies and execute a function. This
     * method is non blocking.
     *
     * @code
     class MyTalent : Talent {
      private:
         Callee myCallee;

      public:
         MyTalent(std::shared_ptr<Adapter> adapter)
             : Talent(MY_TALENT_NAME, adapter) {

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
        auto now_ms = GetEpochTimeMs();
        auto tokens = std::vector<CallToken>{args...};
        auto gatherer = std::make_shared<SinkGatherer>(func, timeout_func, tokens, now_ms);

        reply_handler_->AddGatherer(gatherer);
    }

   protected:
    /**
     * @brief Call the function represented by a Callee, all parameter verification is bypassed.
     *
     * @param callee The callee representing the function to call
     * @param args The arguments to pass to the function
     * @param timeout The maximum time to wait for a reply in ms
     * @return CallToken
     */
    virtual CallToken CallInternal(const Callee& callee, const json& args, int64_t timeout) const;

    const std::string talent_id_;
    const std::string channel_id_;
    const std::string subject_;
    const std::string return_topic_;
    reply_handler_ptr reply_handler_;
    gateway_ptr gateway_;
    uuid_generator_func_ptr uuid_gen_;

};

/**
 * @brief CallContext is the context within which a call originated. The
 * purpose of the context is to be able to trace a chain of events and calls.
 */
class CallContext : public EventContext {
   public:
    virtual ~CallContext() = default;

    /**
     * @brief Construct a new CallContext object
     *
     * @param talent_id The id of the Talent for which this context exists
     * @param channel_id The unique channel ID of the Talent
     * @param feature The name of the feature to emit a return value for (if any)
     * @param event A pointer to the Event to base the context off of
     * @param return_topic The name of the topic to reply to
     * @param reply_handler The ReplyHandler to use for collecting replies to outgoing calls
     * @param gateway A gateway to send replies with
     * @param uuid_gen A function generating stringified UUID4s
     */
    CallContext(const std::string& talent_id, const std::string& channel_id, const std::string& feature,
                event_ptr event, reply_handler_ptr reply_handler, gateway_ptr gateway, uuid_generator_func_ptr uuid_gen);

    virtual CallToken Call(const Callee& callee, const json& args, int64_t timeout = 10000) const override;

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
        auto now_ms = GetEpochTimeMs();
        auto tokens = std::vector<CallToken>{args...};
        auto prepared_reply = PreparedFunctionReply{talent_id_, feature_, event_, return_topic_, gateway_};
        auto gatherer = std::make_shared<ReplyGatherer>(func, timeout_func, prepared_reply, tokens, now_ms);

        reply_handler_->AddGatherer(gatherer);
    }

   private:
    event_ptr event_;
    std::string feature_;
    std::string channel_;
    std::string call_;
    int64_t timeout_at_ms_;
};

using event_ctx_ptr = std::shared_ptr<EventContext>;
using call_ctx_ptr = std::shared_ptr<CallContext>;

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_CONTEXT_HPP_
