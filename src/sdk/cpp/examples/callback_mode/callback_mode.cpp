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

using json = nlohmann::json;
using iotea::core::Service;
using iotea::core::Client;
using iotea::core::Callee;
using iotea::core::event_ctx_ptr;
using iotea::core::call_ctx_ptr;
using iotea::core::AndRules;
using iotea::core::IsSet;
using iotea::core::GreaterThan;
using iotea::core::LessThan;
using iotea::core::ErrorMessage;
using iotea::core::Event;
using iotea::core::schema::rule_ptr;
using iotea::core::logging::NamedLogger;

constexpr char SERVER_ADDRESS[] = "tcp://localhost:1883";

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int) {
    client.Stop();
}

static auto logger = NamedLogger{"CallbackMode"};

int main(int, char**) {
    logger.Error() << "Fired up";

    // Register a global error handler
    client.OnError = [](const ErrorMessage& msg) {
        logger.Error() << "Something went a awry, " << msg.GetMessage();
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
    client.Register(service);


    // Create a stand-alone callee accessible to all
    auto multiply = client.CreateCallee("my_service", "multiply");


    // Create a stand-alone subscription and bind matching events to a function
    client.Subscribe(IsSet("temp", "kuehlschrank"), [](const Event&, event_ctx_ptr) {
        logger.Info() << "The temp is set!";
    });


    // Create another stand-alone subscription and issue a function call upon receving an event
    client.Subscribe(AndRules(GreaterThan("temp", 2, "kuehlschrank"), LessThan("temp", 10, "kuehlschrank")),
            [multiply](const Event& e, event_ctx_ptr ctx) {
        auto value = e.GetValue();

        // Call the previously created callee.
        auto token = ctx->Call(multiply, json{value, value}, 1000);

        auto reply_handler = [](const std::vector<json>& reply) {
            logger.Info() << "kuelschrank.temp=" << reply[0].get<int>(); 
        };
        auto timeout_handler = []{
            logger.Info() << "timed out waiting for result";
        };

        // Defer handling of the function reply until some later point in time.
        ctx->Gather(reply_handler, timeout_handler, token);
    });

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
