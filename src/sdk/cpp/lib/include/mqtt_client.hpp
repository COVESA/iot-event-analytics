/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_MQTT_CLIENT_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_MQTT_CLIENT_HPP_

#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <mqtt/async_client.h>
#pragma GCC diagnostic pop


#include "interface.hpp"
#include "logging.hpp"

namespace iotea {
namespace core {

class MqttClient : public Publisher {
   public:
    MqttClient(const std::string& server_address, const std::string& client_id);
    virtual ~MqttClient() {}

    virtual void Run();
    virtual void Stop();

    // Publisher
    virtual void Publish(const std::string& topic, const std::string& data) override;
    virtual void Subscribe(const std::string& topic, const int qos = 1);

    std::function<void(mqtt::const_message_ptr)> OnMessage;
    std::function<void(int64_t)> OnTick;

   protected:
    /**
     * @brief This constructor should only be used when mocking in unit tests.
     */
    MqttClient();

   private:
    enum class State {
        kDisconnected,
        kConnecting,
        kConnected,
        kStopping,
    };

    std::mutex state_mutex_;
    State state_;
    State next_state_;

    mqtt::connect_options connOpts_;
    mqtt::async_client client_;

    int reconnect_delay_seconds_;
    int64_t prev_tick_;

    std::vector<std::pair<std::string, int>> topics_;

   private:
    void ChangeState(State state);
};

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_MQTT_CLIENT_HPP_
