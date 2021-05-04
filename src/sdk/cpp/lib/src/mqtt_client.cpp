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

#include "util.hpp"
#include "mqtt_client.hpp"

namespace iotea {
namespace core {

using namespace std::chrono_literals;

MqttClient::MqttClient(const std::string& server_address, const std::string& client_id)
    : client_(server_address, client_id) {
    OnMessage = [](mqtt::const_message_ptr msg) {
        (void)msg;
    };
    OnTick = [](int64_t ts){
        (void)ts;
    };

    state_ = State::kDisconnected;
    next_state_ = State::kDisconnected;

    reconnect_delay_seconds_ = 0;

    connOpts_.set_keep_alive_interval(20);
    connOpts_.set_automatic_reconnect(false);
    connOpts_.set_clean_session(true);
    connOpts_.set_connect_timeout(std::chrono::seconds(2));

    client_.start_consuming();
}

MqttClient::MqttClient()
    : client_{"tcp://localhost", "id"} {}

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
                    for (const auto& topic : topics_) {
                        client_.subscribe(topic.first, topic.second);
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
                auto msg = client_.try_consume_message_for(100ms);
                if (msg != nullptr) {
                    OnMessage(msg);
                }

                auto now = GetEpochTimeMs();
                if (now - prev_tick_ >= 1000) {
                    prev_tick_ = now;
                    OnTick(now);
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

void MqttClient::Subscribe(const std::string& topic, const int qos) {
    log::Debug() << "Subscribing to " << topic << " qos=" << qos;
    topics_.push_back(std::make_pair(topic, qos));
    //client_.subscribe(topic, qos);
}

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

void MqttClient::Publish(const std::string& topic, const std::string& data) {
    log::Debug() << "Publishing message.";
    log::Debug() << "\ttopic: '" << topic << "'";
    log::Debug() << "\tpayload: '" << data << "'";
    client_.publish(topic, data);
}

}  // namespace core
}  // namespace iotea
