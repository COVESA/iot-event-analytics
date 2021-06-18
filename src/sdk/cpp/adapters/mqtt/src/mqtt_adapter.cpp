/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <chrono>
#include <memory>
#include <regex>
#include <string>
#include <thread>

#include "mqtt_adapter.hpp"

namespace iotea {
namespace core {

using namespace std::chrono_literals;

static auto logger = iotea::core::logging::NamedLogger{"MqttProtocolAdapter"};

/////////////////////////
// MqttProtocolAdapter //
/////////////////////////

extern "C" {

MqttProtocolAdapter::MqttProtocolAdapter(const std::string& name, bool is_platform_proto, const json& config)
    : Adapter{name, is_platform_proto}
    , client_{config["brokerUrl"].get<std::string>(), "" /* TODO set appropriate client_id */}
    , topic_ns_{config["topicNamespace"].get<std::string>()} {

    state_ = State::kDisconnected;
    next_state_ = State::kDisconnected;

    reconnect_delay_seconds_ = 0;

    connOpts_.set_keep_alive_interval(20);
    connOpts_.set_automatic_reconnect(false);
    connOpts_.set_clean_session(true);
    connOpts_.set_connect_timeout(std::chrono::seconds(2));

    client_.start_consuming();
}

void MqttProtocolAdapter::ChangeState(State state) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (next_state_ != state) {
        switch (state) {
            case State::kDisconnected:
                logger.Debug() << "Changing state to: [Disconnected].";
                break;
            case State::kConnecting:
                logger.Debug() << "Changing state to: [Connecting].";
                break;
            case State::kConnected:
                logger.Debug() << "Changing state to: [Connected].";
                break;
            case State::kStopping: {
                logger.Debug() << "Changing state to: [Stopping].";
            } break;
        }
        next_state_ = state;
    }
}

void MqttProtocolAdapter::Start() {
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
                    logger.Info() << "Connecting to '" << client_.get_server_uri() << "'... ";
                    connect_token = client_.connect(connOpts_);
                    break;
                case State::kConnected: {
                    logger.Info() << "Connected";
                    connect_token = nullptr;
                    for (const auto& topic : topics_) {
                        client_.subscribe(topic, 1 /* qos */);
                    }
                } break;
                case State::kDisconnected:
                    logger.Info() << "Disconnected";
                    reconnect_delay_seconds_ = 5;
                    break;
                case State::kStopping:
                    if (client_.is_connected()) {
                        logger.Info() << "Disconnecting... ";
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
                auto msg = client_.try_consume_message_for(100ms);
                if (msg != nullptr) {
                    auto topic = msg->get_topic();

                    for (const auto& m : matchers_) {
                        if (m.first.Match(topic)) {
                            m.second(topic, msg->get_payload_str(), "");
                        }
                    }
                }

            } break;
            case State::kConnecting: {
                try {
                    if (connect_token->wait_for(5s)) {
                        ChangeState(State::kConnected);
                    }
                } catch (const mqtt::exception& e) {
                    logger.Error() << "Unable to connect to MQTT server '" << client_.get_server_uri()
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
                    logger.Debug() << "Wait up to 5 seconds for disconnect confirmation.";
                    if (disconnect_token->wait_for(5s)) {
                        logger.Debug() << "Disconnected successfully.";
                    } else {
                        logger.Debug() << "Failed to get disconnect confirmation.";
                    }
                }
                running = false;
            } break;
        }
    }
}

void MqttProtocolAdapter::Stop() {
    ChangeState(State::kStopping);
}

void MqttProtocolAdapter::Publish(const std::string& topic, const std::string& data, const PublishOptions&) {
    auto full_topic = topic_ns_ + topic;

    logger.Debug() << "Publishing message.";
    logger.Debug() << "\ttopic: '" << full_topic << "'";
    logger.Debug() << "\tpayload: '" << data << "'";

    client_.publish(full_topic, data);
}

void MqttProtocolAdapter::Subscribe(const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions&) {
    auto topic_with_ns = topic_ns_ + topic;

    logger.Debug() << "Subscribing to " << topic_with_ns;
    topics_.push_back(topic_with_ns);
    matchers_.push_back(std::make_pair(TopicExprMatcher{topic_with_ns}, on_msg));
}

void MqttProtocolAdapter::SubscribeShared(const std::string& group, const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions&) {
    auto topic_with_ns = topic_ns_ + topic;
    auto shared_topic = std::string{"$share"} + "/" + group + "/" + topic_with_ns;

    logger.Debug() << "Subscribing to " << shared_topic;
    topics_.push_back(shared_topic);
    matchers_.push_back(std::make_pair(TopicExprMatcher{topic_with_ns}, on_msg));
}

} // extern "C"

}  // namespace core
}  // namespace iotea

extern "C" std::shared_ptr<iotea::core::Adapter> Load(const std::string& name, bool is_platform_proto, const json& config) {
    return std::make_shared<iotea::core::MqttProtocolAdapter>(name, is_platform_proto, config);
}
