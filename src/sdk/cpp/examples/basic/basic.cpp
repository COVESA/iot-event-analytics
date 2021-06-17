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
#include <memory>

#include "nlohmann/json.hpp"

#include "client.hpp"
#include "protocol_gateway.hpp"

using json = nlohmann::json;
using iotea::core::ProtocolGateway;
using iotea::core::FunctionTalent;
using iotea::core::Talent;
using iotea::core::Client;
using iotea::core::Callee;
using iotea::core::error_message_ptr;
using iotea::core::event_ptr;
using iotea::core::event_ctx_ptr;
using iotea::core::Change;
using iotea::core::IsSet;
using iotea::core::GreaterThan;
using iotea::core::LessThan;
using iotea::core::schema::rule_ptr;


class MyService : public Talent {
   public:
    MyService() : Talent("cpp-basic-talent") { }

    rule_ptr OnGetRules() const override {
        return Change("anyfeature", "anytype");
    }

    void OnEvent(event_ptr event, event_ctx_ptr) override {
        GetLogger().Info() << "Event: " << event->GetValue().dump(4);
    }

    void OnError(error_message_ptr msg) override {
        GetLogger().Error() << "Something went a awry, " << msg->GetMessage();
    };
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

    client->RegisterTalent(std::make_shared<MyService>());

    std::signal(SIGINT, signal_handler);
    client->Start();

    return 0;
}
