/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "mqtt_client.hpp"
#pragma GCC diagnostic pop

#include <memory>
#include <regex>
#include <string>
#include <thread>

#include "util.hpp"

namespace iotea {
namespace core {

using namespace std::chrono_literals;

const int QOS = 1;
const char TALENTS_DISCOVERY_TOPIC[] = "configManager/talents/discover";
const char INGESTION_EVENTS_TOPIC[] = "ingestion/events";
const char PLATFORM_EVENTS_TOPIC[] = "platform/$events";
const char MQTT_TOPIC_NS[] = "MQTT_TOPIC_NS";

MqttClient::MqttClient(const std::string& server_address, const std::string& client_id)
    : client_(server_address, client_id)
    , mqtt_topic_ns_{GetEnv(MQTT_TOPIC_NS, "iotea")} {
    state_ = State::kDisconnected;
    next_state_ = State::kDisconnected;

    reconnect_delay_seconds_ = 0;

    connOpts_.set_keep_alive_interval(20);
    connOpts_.set_automatic_reconnect(false);
    connOpts_.set_clean_session(true);
    connOpts_.set_connect_timeout(std::chrono::seconds(2));

    client_.start_consuming();
}

void MqttClient::Run() {
    mqtt::token_ptr connect_token;
    mqtt::token_ptr disconnect_token;
    bool running = true;

    while (running) {
        if (next_state_ != state_) {
            // on_exit
            switch (state_) {
                default:
                    break;
            }
            // on entry
            switch (next_state_) {
                case State::kConnecting:
                    log::Info() << "Connecting to '" << client_.get_server_uri() << "'... ";
                    connect_token = client_.connect(connOpts_);
                    break;
                case State::kConnected: {
                    log::Info() << "Connected";
                    connect_token = nullptr;
                    for (const auto& talent : talents_) {
                        auto discover_topic = GetDiscoverTopic();
                        auto platform_topic = GetPlatformEventsTopic();
                        auto shared_prefix = GetSharedPrefix(talent.second->GetId());
                        auto event_topic = GetEventTopic(talent.second->GetId());

                        // Discover
                        client_.subscribe(shared_prefix + "/" + discover_topic, QOS);
                        // Platform
                        client_.subscribe(shared_prefix + "/" + platform_topic, QOS);
                        // Event
                        client_.subscribe(shared_prefix + "/" + event_topic, QOS);
                        // Call
                        client_.subscribe(event_topic + "/" + talent.second->GetChannel() + "/+", QOS);
                    }
                } break;
                case State::kDisconnected:
                    log::Info() << "Disconnected";
                    reconnect_delay_seconds_ = 5;
                    break;
                case State::kStopping:
                    if (client_.is_connected()) {
                        log::Info() << "Disconnecting... ";
                        disconnect_token = client_.disconnect();
                    }
                    break;
            }
            state_ = next_state_;
        }
        switch (state_) {
            case State::kConnected: {
                if (!client_.is_connected()) {
                    ChangeState(State::kDisconnected);
                }
                auto msg = client_.try_consume_message_for(1s);
                if (msg != nullptr) {
                    OnMessage(msg);
                }
            } break;
            case State::kConnecting: {
                try {
                    if (connect_token->wait_for(5s)) {
                        ChangeState(State::kConnected);
                    }
                } catch (const mqtt::exception& e) {
                    log::Error() << "Unable to connect to MQTT server '" << client_.get_server_uri()
                                 << "': " << e.to_string();
                    ChangeState(State::kDisconnected);
                }
            } break;
            case State::kDisconnected: {
                if (reconnect_delay_seconds_ > 0) {
                    reconnect_delay_seconds_--;
                    std::this_thread::sleep_for(1s);
                } else {
                    ChangeState(State::kConnecting);
                }
            } break;

            case State::kStopping: {
                if (disconnect_token) {
                    log::Debug() << "Wait up to 5 seconds for disconnect confirmation.";
                    if (disconnect_token->wait_for(5s)) {
                        log::Debug() << "Disconnected successfully.";
                    } else {
                        log::Debug() << "Failed to get disconnect confirmation.";
                    }
                }
                running = false;
            } break;
        }
    }
}

void MqttClient::Stop() { ChangeState(State::kStopping); }

void MqttClient::ChangeState(State state) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (next_state_ != state) {
        switch (state) {
            case State::kDisconnected:
                log::Debug() << "Changing state to: [Disconnected].";
                break;
            case State::kConnecting:
                log::Debug() << "Changing state to: [Connecting].";
                break;
            case State::kConnected:
                log::Debug() << "Changing state to: [Connected].";
                break;
            case State::kStopping: {
                log::Debug() << "Changing state to: [Stopping].";
            } break;
        }
        next_state_ = state;
    }
}

void MqttClient::RegisterTalent(std::shared_ptr<Talent> talent) { talents_[talent->GetId()] = talent; }

// Callback for when a message arrives.
void MqttClient::OnMessage(mqtt::const_message_ptr msg) {
    log::Debug() << "Message arrived.";
    log::Debug() << "\ttopic: '" << msg->get_topic() << "'";
    log::Debug() << "\tpayload: '" << msg->to_string();

    std::cmatch m;

    // Forward event
    // Received events look like this {MQTT_TOPIC_NS}(remote/)talent/<talentId>/events
    // In the regex below we assume that both instance of <talentId> are the same
    static const auto event_expr = std::regex{"^" + mqtt_topic_ns_ + R"(/(?:remote/)?talent/([^/]+)/events$)"};
    if (std::regex_match(msg->get_topic().c_str(), m, event_expr)) {
        OnEvent(m[1], msg);
        return;
    }

    // iotea/talent/event_consumer/events/channel/callid
    // Forward deferred call response
    // (remote/)talent/<talentId>/events/<callChannelId>/<deferredCallId>
    static const auto call_expr =
        std::regex{"^" + mqtt_topic_ns_ + R"(/(?:remote/)?talent/([^/]+)/events/([^/]+)/([^/]+)$)"};
    if (std::regex_match(msg->get_topic().c_str(), m, call_expr)) {
        OnDeferredCall(m[1], m[2], m[3], msg);
        return;
    }

    // Forward discovery request
    if (msg->get_topic() == GetDiscoverTopic()) {
        OnDiscover(msg);
        return;
    }

    // Forward platform request
    if (msg->get_topic() == GetPlatformEventsTopic()) {
        OnPlatformEvent(msg);
        return;
    }

    // TODO log error: could not make sense of topic
}

void MqttClient::OnDiscover(mqtt::const_message_ptr msg) {
    log::Debug() << "Received discovery message.";
    auto payload = msg->to_string();

    for (const auto& talent : talents_) {
        talent.second->HandleDiscover(payload);
    }
}

void MqttClient::OnPlatformEvent(mqtt::const_message_ptr msg) {
    log::Debug() << "Received platform message.";
    auto payload = msg->to_string();

    for (const auto& talent : talents_) {
        talent.second->HandlePlatformEvent(payload);
    }
}

void MqttClient::OnEvent(const std::string& talent_id, mqtt::const_message_ptr msg) {
    log::Debug() << "Received event message.";
    auto talent = talents_.find(talent_id);
    if (talent != talents_.end()) {
        talent->second->HandleEvent(msg->to_string());
        return;
    }

    log::Info() << "Received event for unregistered talent";
}

void MqttClient::OnDeferredCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                                mqtt::const_message_ptr msg) {
    auto payload = msg->to_string();

    auto talent = talents_.find(talent_id);
    if (talent != talents_.end()) {
        talent->second->HandleDeferredCall(channel_id, call_id, payload);
        return;
    }

    log::Info() << "Received reply destined for unregistered talent";
}

std::string MqttClient::GetDiscoverTopic() const { return mqtt_topic_ns_ + "/" + TALENTS_DISCOVERY_TOPIC; }

std::string MqttClient::GetSharedPrefix(const std::string& talent_id) const { return "$share/" + talent_id; }

std::string MqttClient::GetEventTopic(const std::string& talent_id) const {
    return mqtt_topic_ns_ + "/talent/" + talent_id + "/events";
}

std::string MqttClient::GetPlatformEventsTopic() const {
    return mqtt_topic_ns_+ "/" + PLATFORM_EVENTS_TOPIC;
}

void MqttClient::Publish(const std::string& topic, const std::string& data) {
    log::Debug() << "Publishing message.";
    log::Debug() << "\ttopic: '" << GetIngestionEventsTopic() << "'";
    log::Debug() << "\tpayload: '" << data << "'";
    client_.publish(topic, data);
}

std::string MqttClient::GetIngestionEventsTopic() const { return GetNamespace() + "/" + INGESTION_EVENTS_TOPIC; }

std::string MqttClient::GetNamespace() const { return mqtt_topic_ns_; }

}  // namespace core
}  // namespace iotea
