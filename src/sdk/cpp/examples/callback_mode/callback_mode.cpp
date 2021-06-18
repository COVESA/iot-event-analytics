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
#include <iostream>
#include "nlohmann/json.hpp"

#include "client.hpp"
#include "protocol_gateway.hpp"

using json = nlohmann::json;
using iotea::core::ProtocolGateway;
using iotea::core::Service;
using iotea::core::Client;
using iotea::core::Callee;
using iotea::core::call_ctx_ptr;
using iotea::core::error_message_ptr;
using iotea::core::event_ptr;
using iotea::core::event_ctx_ptr;
using iotea::core::AndRules;
using iotea::core::IsSet;
using iotea::core::GreaterThan;
using iotea::core::LessThan;
using iotea::core::schema::rule_ptr;
using iotea::core::logging::NamedLogger;

std::shared_ptr<Client> client;

void signal_handler(int) {
    if (client) {
        client->Stop();
    }
}

static auto logger = NamedLogger("CallbackMode");

int main(int, char** argv) {
    logger.Error() << "Fired up";

    std::ifstream file{argv[1]};
    std::string config{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto gateway = std::make_shared<ProtocolGateway>(json::parse(config));
    client = std::make_shared<Client>(gateway);

    // Register a global error handler
    client->OnError = [](error_message_ptr msg) {
        logger.Error() << "Something went a awry, " << msg->GetMessage();
    };


    // Service mechanism
    auto service = Service{"my_service"};


    // "Dynamically" add a function to the service
    service.RegisterFunction("multiply", [](const json& args, call_ctx_ptr ctx) {
        auto a = args[0].get<int>();
        auto b = args[1].get<int>();

        ctx->Reply(a * b);
    });


    // Register service with client
    client->Register(service);


    // Create a stand-alone callee accessible to all
    auto multiply = client->CreateCallee("my_service", "multiply");


    // Create a stand-alone subscription and bind matching events to a function
    client->Subscribe(IsSet("anyfeature", "anytype"), [](event_ptr, event_ctx_ptr) {
        logger.Info() << "anyfeature is set!";
    });


    // Create another stand-alone subscription and issue a function call upon receving an event
    client->Subscribe(AndRules(GreaterThan("anyfeature", 2, "anytype"), LessThan("anyfeature", 10, "anytype")),
            [multiply](event_ptr e, event_ctx_ptr ctx) {
        auto value = e->GetValue();

        // Call the previously created callee.
        auto token = ctx->Call(multiply, json{value, value}, 1000);

        auto reply_handler = [](const std::vector<json>& reply) {
            logger.Info() << "anytype.anyfeature=" << reply[0].get<int>();
        };
        auto timeout_handler = []{
            logger.Info() << "timed out waiting for result";
        };

        // Defer handling of the function reply until some later point in time.
        ctx->Gather(reply_handler, timeout_handler, token);
    });

    std::signal(SIGINT, signal_handler);
    client->Start();

    return 0;
}
