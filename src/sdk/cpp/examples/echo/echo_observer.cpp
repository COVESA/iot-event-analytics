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
#include <fstream>
#include <iostream>
#include <iterator>
#include <initializer_list>
#include <memory>

#include "nlohmann/json.hpp"
#include "client.hpp"
#include "protocol_gateway.hpp"
#include "schema.hpp"

using namespace iotea::core;
using json = nlohmann::json;

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

    void OnEvent(event_ptr event, event_ctx_ptr) override {
        if (event->GetFeature() == SUBSCRIBED_ECHO_EVENT) {
            auto message = event->GetValue().get<std::string>();
            GetLogger().Info() << "Received echo: '" << message << "'";
        } else if (event->GetFeature() == SUBSCRIBED_COUNT_EVENT) {
            auto echoCount = event->GetValue().get<unsigned int>();
            GetLogger().Info() << "Received echoCount: " << echoCount;
        } else {
            GetLogger().Warn() << "UNKNOWN EVENT RECEIVED";
        }
    }
};

std::shared_ptr<Client> client;

void signal_handler(int) {
    if (client) {
        client->Stop();
    }
}

int main(int, char** argv) {
    std::ifstream file{argv[1]};
    std::string config{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto gateway = std::make_shared<ProtocolGateway>(json::parse(config));
    client = std::make_shared<Client>(gateway);

    client->RegisterTalent(std::make_shared<EchoObserver>());

    std::signal(SIGINT, signal_handler);
    client->Start();

    return 0;
}
