/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <csignal>
#include <initializer_list>
#include <memory>

#include "nlohmann/json.hpp"
#include "iotea.hpp"
#include "logging.hpp"
#include "mqtt_client.hpp"
#include "schema.hpp"

using namespace iotea::core;
using json = nlohmann::json;

static const std::string SERVER_ADDRESS("tcp://localhost:1883");
static const std::string TALENT_NAME("echo_observer");
static const std::string PROVIDER_TALENT_NAME("echo_provider");
static const std::string SUBSCRIBED_ECHO_EVENT(PROVIDER_TALENT_NAME+".echoResponseSent");
static const std::string SUBSCRIBED_COUNT_EVENT(PROVIDER_TALENT_NAME+".echoCount");


class EchoObserver : public Talent {
   public:
    EchoObserver()
        : Talent(TALENT_NAME) {
    }

    schema::rule_ptr OnGetRules() const override {
        return OrRules(IsSet(SUBSCRIBED_ECHO_EVENT), IsSet(SUBSCRIBED_COUNT_EVENT));
    }

    void OnEvent(const Event& event, event_ctx_ptr) override {
        if (event.GetFeature() == SUBSCRIBED_ECHO_EVENT) {
            auto message = event.GetValue().get<std::string>();
            log::Info() << "Received echo: '" << message << "'";
        } else if (event.GetFeature() == SUBSCRIBED_COUNT_EVENT) {
            auto echoCount = event.GetValue().get<unsigned int>();
            log::Info() << "Received echoCount: " << echoCount;
        } else {
            log::Warn() << "UNKNOWN EVENT RECEIVED";
        }
    }
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int) {
    client.Stop();
}

int main(int, char**) {
    auto talent = std::make_shared<EchoObserver>();
    client.RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
