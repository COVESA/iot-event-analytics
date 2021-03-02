/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#include <csignal>
#include <iostream>
#include <memory>

#include "nlohmann/json.hpp"
#include "logging.hpp"
#include "mqtt_client.hpp"

using json = nlohmann::json;
using namespace iotea::core;

static const char SERVER_ADDRESS[] = "tcp://localhost:1883";
static const char FEATURE[] = "testable_talent";
static const char FUNC_ECHO[] = "echo";

class EchoTalent : public FunctionTalent {
   public:
    explicit EchoTalent(std::shared_ptr<Publisher> publisher)
        : FunctionTalent(FEATURE, publisher) {
        RegisterFunction(FUNC_ECHO, [](const json& args, const CallContext& context) {
            context.Reply(args[0]);
        });
        SkipCycleCheck(true);
    }
};

static std::shared_ptr<MqttClient> client = std::make_shared<MqttClient>(SERVER_ADDRESS, FEATURE);

void signal_handler(int signal) { client->Stop(); }

int main(int argc, char* argv[]) {
    auto talent = std::make_shared<EchoTalent>(client);
    client->RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);

    client->Run();

    return 0;
}
