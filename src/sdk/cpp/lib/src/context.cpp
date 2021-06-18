/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "common.hpp"
#include "context.hpp"
#include "logging.hpp"

namespace iotea {
namespace core {

//
// EventContext
//
EventContext::EventContext(const std::string& talent_id, const std::string& channel_id, const std::string& subject,
                           const std::string& return_topic, reply_handler_ptr reply_handler, gateway_ptr gateway, uuid_generator_func_ptr uuid_gen)
    : talent_id_{talent_id}
    , channel_id_{channel_id}
    , subject_{subject}
    , return_topic_{return_topic}
    , reply_handler_{reply_handler}
    , gateway_{gateway}
    , uuid_gen_{uuid_gen} {}

std::string EventContext::GetChannelId() const { return channel_id_; }

std::string EventContext::GetSubject() const { return subject_; }

std::string EventContext::GetReturnTopic() const {
    return return_topic_;
}

static auto call_token_logger = iotea::core::logging::NamedLogger("CallToken");

CallToken EventContext::Call(const Callee& callee, const json& args, int64_t timeout) const {
    if (!callee.IsRegistered()) {
        call_token_logger.Warn() << "Tried to call unregistered Callee";

        // TODO how do we best report an error in this case? We would like to avoid using exceptions.
        return CallToken{"", -1};
    }

    if (timeout <= 0) {
        // Oops this call has already timed out.
        throw std::logic_error("timeout must be larger that 0");
    }

    return CallInternal(callee, args, timeout);
}

CallToken EventContext::CallInternal(const Callee& callee, const json& args, int64_t timeout) const {
    auto call_id = uuid_gen_();
    auto j = args.is_array() ? args : json::array({args});
    auto c = OutgoingCall{callee.GetTalentId(), GetChannelId(), call_id, callee.GetFunc(), j, GetSubject(), callee.GetType(), timeout};

    gateway_->Publish(GetReturnTopic(), c.Json().dump());

    return CallToken{call_id, timeout};
}


//
// CallContext
//
CallContext::CallContext(const std::string& talent_id, const std::string& channel_id, const std::string& feature,
                         event_ptr event, reply_handler_ptr reply_handler, gateway_ptr gateway, uuid_generator_func_ptr uuid_gen)
    : EventContext{talent_id, channel_id, event->GetSubject(), event->GetReturnTopic(), reply_handler, gateway, uuid_gen}
    , event_{event}
    , feature_{feature}
    , channel_{event->GetValue()["chnl"].get<std::string>()}
    , call_{event->GetValue()["call"].get<std::string>()}
    , timeout_at_ms_{event->GetValue()["timeoutAtMs"].get<int64_t>()} {}


CallToken CallContext::Call(const Callee& callee, const json& args, int64_t timeout) const {
    // If Call is called from a CallContext that means that it's part of a
    // chain of calls. In that case we need to take a look at the "timeoutAtMs"
    // property (which holds the absolute timeout of the first call in the
    // chain) and modify the supplied timeout so that it doesn't expire after
    // "timeoutAtMs".

    // The timeout argument is in relative time and must be adjusted with
    // respect to the absolute timeout given in the original call so that it
    // doesn't expire after it. If the result of the subtraction in the second
    // argument is negative we still create the CallToken but don't issue
    // the actual call (taken care of in EventContext::Call). The token will
    // cause the gatherer to expire in the next
    // "check timeouts cycle".
    timeout = std::min(timeout, timeout_at_ms_ - GetEpochTimeMs());

    if (timeout <= 0) {
        // This call has already timed out. Just reply with a token that times
        // out immediately and let the gatherer take care of handling the
        // timeout.
        return CallToken{uuid_gen_(), 0};
    }

    return CallInternal(callee, args, timeout);
}

void CallContext::Reply(const json& value) const {
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

}  // namespace core
}  // namespace iotea
