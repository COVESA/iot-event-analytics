/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_CALL_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_CALL_HPP_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "nlohmann/json.hpp"

#include "common.hpp"
#include "util.hpp"

using json = nlohmann::json;

namespace iotea {
namespace core {

using call_id_t = std::string;

using gather_func_ptr = std::function<void(std::vector<json>)>;
using gather_and_reply_func_ptr = std::function<json(std::vector<json>)>;
using timeout_func_ptr = std::function<void(void)>;

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
    * @param timeout The number of ms to wait for a reply to the call
    * @param when The time since the epoch in ms to attach to the call
    */
    OutgoingCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                 const std::string& func, const json& args, const std::string& subject, const std::string& type,
                 int64_t timeout, int64_t when = GetEpochTimeMs());

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
    int64_t timeout_;
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

    /**
     * @brief Compare this Callee to another. Comparison ignores the "registerd_" member.
     *
     * @param other The Callee to compare this Callee to
     *
     * @return true If this Callee is equal to "other"
     */
    bool operator==(const Callee& other) const;

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
     * @param now_ms The current time in ms since the epoch
     */
    Gatherer(timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms = 0);

    virtual ~Gatherer() = default;

    /**
     * @brief Check if any of the pending calls have timed out.
     *
     * @param now_ms The current time in ms since the epoch
     * @return true if at least one call has timed out.
     */
    virtual bool HasTimedOut(int64_t now_ms) const;

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

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_CALL_HPP_
