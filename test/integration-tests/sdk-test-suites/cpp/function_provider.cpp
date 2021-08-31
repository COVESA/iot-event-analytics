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
#include <memory>

#include "nlohmann/json.hpp"
#include "client.hpp"

using json = nlohmann::json;

using iotea::core::Client;
using iotea::core::FunctionTalent;
using iotea::core::ProtocolGateway;
using iotea::core::call_ctx_ptr;

static const char TALENT_ID[] = "functionProvider-cpp";
static const char FUNC_ECHO[] = "echo";

class FunctionProvider : public FunctionTalent {
   public:
    FunctionProvider()
        : FunctionTalent(TALENT_ID) {
        RegisterFunction(FUNC_ECHO, [](const json& args, call_ctx_ptr context) {
            context->Reply(args[0]);
        });
    }
};

std::shared_ptr<Client> client;

void signal_handler(int) {
    if (client) {
        client->Stop();
    }
}

int main(int, char**) {
    std::ifstream file{"../../../config/tests/cpp/config.json"};
    std::string config{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

    auto gateway = std::make_shared<ProtocolGateway>(json::parse(config));
    client = std::make_shared<Client>(gateway);

    auto talent = std::make_shared<FunctionProvider>();
    client->RegisterFunctionTalent(talent);

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    client->Start();

    return 0;
}
