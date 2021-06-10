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
#include <iostream>
#include "nlohmann/json.hpp"

#include "client.hpp"
#include "mqtt_client.hpp"

using json = nlohmann::json;
using iotea::core::FunctionTalent;
using iotea::core::Talent;
using iotea::core::Client;
using iotea::core::Callee;
using iotea::core::event_ctx_ptr;
using iotea::core::Change;
using iotea::core::IsSet;
using iotea::core::GreaterThan;
using iotea::core::LessThan;
using iotea::core::ErrorMessage;
using iotea::core::Event;
using iotea::core::schema::rule_ptr;

namespace logging = iotea::core::log;

constexpr char SERVER_ADDRESS[] = "tcp://localhost:1883";

class MyService : public Talent {
   public:
    MyService() : Talent("cpp-basic-talent") { }

    rule_ptr OnGetRules() const override {
        return Change("temp", "kuehlschrank");
    }

    void OnEvent(const Event& event, event_ctx_ptr) override {
        logging::Info() << "Event: " << event.GetValue().dump(4);
    }

    void OnError(const ErrorMessage& msg) override {
        logging::Error() << "Something went a awry, " << msg.GetMessage(); 
    };
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int) {
    client.Stop();
}

int main(int, char**) {
    client.RegisterTalent(std::make_shared<MyService>());

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
