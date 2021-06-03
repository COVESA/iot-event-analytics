/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_ADAPTERS_MQTT_HPP_
#define SRC_SDK_CPP_ADAPTERS_MQTT_HPP_

#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <mqtt/async_client.h>
#pragma GCC diagnostic pop

#include "nlohmann/json.hpp"

using json = nlohmann::json;

#include "logging.hpp"
#include "protocol_gateway.hpp"
#include "util.hpp"

namespace iotea {
namespace core {

extern "C" {
class MqttProtocolAdapter : public Adapter {
   public:
    MqttProtocolAdapter(const std::string& name, bool is_platform_proto, const json& config);

    virtual ~MqttProtocolAdapter() = default;

    // Adapter
    void Start() override;
    void Stop() override;
    void Publish(const std::string& topic, const std::string& data, const PublishOptions& opts) override;
    void Subscribe(const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) override;
    void SubscribeShared(const std::string& group, const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) override;

   private:
    enum class State {
        kDisconnected,
        kConnecting,
        kConnected,
        kStopping,
    };

    void ChangeState(State state);

    mqtt::async_client client_;
    std::string topic_ns_;
    mqtt::connect_options connOpts_;

    on_msg_func_ptr on_msg_;
    int reconnect_delay_seconds_;

    std::vector<std::string> topics_;
    std::vector<std::pair<TopicExprMatcher, on_msg_func_ptr>> matchers_;

    std::mutex state_mutex_;
    State state_;
    State next_state_;
};

} // extern "C"

}  // namespace core
}  // namespace iotea

extern "C" std::shared_ptr<iotea::core::Adapter> Load(const std::string& name, bool is_platform_proto, const json& config);

#endif // SRC_SDK_CPP_ADAPTERS_MQTT_HPP_
