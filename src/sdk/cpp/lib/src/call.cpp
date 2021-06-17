/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "call.hpp"
#include "logging.hpp"

using iotea::core::logging::NamedLogger;

namespace iotea {
namespace core {

//
// OutgoingCall
//
OutgoingCall::OutgoingCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                           const std::string& func, const json& args, const std::string& subject,
                           const std::string& type, int64_t timeout, int64_t when)
    : talent_id_{talent_id}
    , channel_id_{channel_id}
    , call_id_{call_id}
    , func_{func}
    , args_(args)
    , subject_{subject}
    , type_{type}
    , timeout_{when + timeout}
    , when_{when} {}

std::string OutgoingCall::GetCallId() const { return call_id_; }

json OutgoingCall::Json() const {
    return json{{"subject", subject_},
                {"feature", talent_id_ + "." + func_ + "-in"},
                {"type", type_},
                {"value", json{
                      {"func", func_},
                      {"args", args_},
                      {"call", call_id_},
                      {"chnl", channel_id_},
                      {"timeoutAtMs", timeout_}
                }},
                {"whenMs", when_}};
}

//
// CallToken
//
static auto call_token_logger = NamedLogger("CallToken");

CallToken::CallToken(const std::string& call_id, int64_t timeout)
    : call_id_{call_id}
    , timeout_{timeout} {}

std::string CallToken::GetCallId() const {
    return call_id_;
}

int64_t CallToken::GetTimeout() const {
    return timeout_;
}

//
// Gatherer
//
static auto gatherer_logger = NamedLogger("Gatherer");

Gatherer::Gatherer(timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms)
    : timeout_func_{timeout_func} {

    auto nearest_timeout = std::numeric_limits<int64_t>::max();

    for (const auto& t : tokens) {
        ids_.insert(t.GetCallId());

        nearest_timeout = std::min(nearest_timeout, t.GetTimeout());
    }

    timeout_ = now_ms + nearest_timeout;
}

bool Gatherer::HasTimedOut(int64_t now_ms) const {
    return timeout_ <= now_ms;
}

void Gatherer::TimeOut() {
    if (timeout_func_) {
        timeout_func_();
    }
}

bool Gatherer::Wants(const call_id_t& id) const {
    return ids_.find(id) != ids_.end();
}

bool Gatherer::IsReady() const {
    return ids_.size() == replies_.size();
}

bool Gatherer::Gather(const call_id_t& id, const json& reply) {
    if (ids_.find(id) == ids_.end()) {
        gatherer_logger.Error() << "Unrecognized call id " << id;
        return false;
    }

    replies_[id] = reply;

    return IsReady();
}

std::vector<json> Gatherer::GetReplies() const {
    std::vector<json> replies;

    for (const auto& t : ids_) {
        replies.push_back(replies_.find(t)->second);
    }

    return replies;
}

//
// SinkGatherer
//
SinkGatherer::SinkGatherer(gather_func_ptr func, timeout_func_ptr timeout_func, const std::vector<CallToken>& tokens, int64_t now_ms)
            : Gatherer{timeout_func, tokens, now_ms}
            , func_{func} {}

void SinkGatherer::ForwardReplies(const std::vector<json>& replies) const {
    func_(replies);
}

//
// PreparedFunctionReply
//
PreparedFunctionReply::PreparedFunctionReply(const std::string& talent_id,
        const std::string& feature,
        event_ptr event,
        const std::string& return_topic,
        gateway_ptr gateway)
    : talent_id_{talent_id}
    , feature_{feature}
    , event_{event}
    , return_topic_{return_topic}
    , gateway_{gateway} {}

void PreparedFunctionReply::Reply(const json& value) const {

    auto channel = event_->GetValue()["chnl"].get<std::string>();
    auto call = event_->GetValue()["call"].get<std::string>();
    auto result = json{
        {"$tsuffix", std::string("/") + channel + "/" + call},
        {"$vpath", "value"},
        {"value", value}
    };

    auto subject = event_->GetSubject();
    auto type = event_->GetType();
    auto instance = event_->GetInstance();
    auto event = OutgoingEvent<json>{subject, talent_id_, talent_id_ + "." + feature_, result, type, instance};

    gateway_->Publish(return_topic_, event.Json().dump());
}

//
// ReplyGatherer
//
ReplyGatherer::ReplyGatherer(gather_and_reply_func_ptr func, timeout_func_ptr timeout_func, const PreparedFunctionReply& prepared_reply, const std::vector<CallToken>& tokens, int64_t now_ms)
    : Gatherer{timeout_func, tokens, now_ms}
    , func_{func}
    , prepared_reply_{prepared_reply} {}

void ReplyGatherer::ForwardReplies(const std::vector<json>& replies) const {
    auto value = func_(replies);
    prepared_reply_.Reply(value);
}

//
// Callee
//
Callee::Callee()
    : registered_(false) {}

Callee::Callee(const std::string& talent_id, const std::string& func, const std::string& type)
    : talent_id_{talent_id}
    , func_{func}
    , type_{type} {
    registered_ = true;  // Should probably be deferred until talent is actually registered
}

std::string Callee::GetFeature() const { return talent_id_ + "." + func_; }

std::string Callee::GetFunc() const { return func_; }

std::string Callee::GetTalentId() const { return talent_id_; }

std::string Callee::GetType() const { return type_; }

bool Callee::IsRegistered() const {
    return registered_;
}

bool Callee::operator==(const Callee& other) const {
    return talent_id_ == other.talent_id_ &&
        func_ == other.func_ &&
        type_ == other.type_;
}

//
// ReplyHandler
//
void ReplyHandler::AddGatherer(std::shared_ptr<Gatherer> gatherer) {
    gatherers_.push_back(gatherer);
}

std::shared_ptr<Gatherer> ReplyHandler::ExtractGatherer(const call_id_t& call_id) {
    auto it = std::find_if(gatherers_.begin(), gatherers_.end(), [call_id](const auto& g) {
        return g->Wants(call_id);
    });

    if (it == gatherers_.end()) {
        return nullptr;
    }

    auto g = *it;
    gatherers_.erase(it);

    return g;
}

std::vector<std::shared_ptr<Gatherer>> ReplyHandler::ExtractTimedOut(int64_t ts) {
    std::vector<std::shared_ptr<Gatherer>> timed_out;

    auto it_end = std::remove_if(gatherers_.begin(), gatherers_.end(), [&timed_out, ts](const auto& g) {
            auto has_timed_out = g->HasTimedOut(ts);

            if (has_timed_out) {
                timed_out.push_back(g);
            }

            return has_timed_out;
        });

    gatherers_.erase(it_end, gatherers_.end());

    return timed_out;
}
}  // namespace core
}  // namespace iotea

