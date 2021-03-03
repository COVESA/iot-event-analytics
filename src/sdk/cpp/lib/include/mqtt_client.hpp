/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#include <mqtt/async_client.h>

#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "iotea.hpp"

namespace iotea {
namespace core {

class MqttClient : public Publisher {
   public:
    MqttClient(const std::string& server_address, const std::string& client_id);
    virtual ~MqttClient() {}

    void Run();
    void Stop();

    void RegisterTalent(std::shared_ptr<Talent> talent);

    // Publisher
    void Publish(const std::string& topic, const std::string& data) override;
    std::string GetIngestionTopic() const override;
    std::string GetNamespace() const override;

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
    std::string discover_topic_;

    std::map<std::string, std::shared_ptr<Talent>> talents_;

   private:
    void ChangeState(State state);
    void OnMessage(mqtt::const_message_ptr msg);
    void OnDiscover(mqtt::const_message_ptr msg);
    void OnEvent(const std::string& talent_id, mqtt::const_message_ptr msg);
    void OnDeferredCall(const std::string& talent_id, const std::string& channel_id, const std::string& call_id,
                        mqtt::const_message_ptr msg);

    std::string GetSharedPrefix(const std::string& talent_id) const;
    std::string GetDiscoverTopic() const;
    std::string GetEventTopic(const std::string& talent_id) const;

    std::string get_event_topic(std::shared_ptr<Talent> talent);

    const std::string mqtt_topic_ns_;
};

}  // namespace core
}  // namespace iotea
