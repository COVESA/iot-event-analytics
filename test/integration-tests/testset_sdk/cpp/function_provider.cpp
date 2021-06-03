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

static auto mqtt_config = json{
    {"platform", true},
    {"module", {
                   {"name", "./testset_sdk/cpp/build/iotea-sdk-cpp-lib/adapters/mqtt/libmqtt_protocol_adapter.so"}
               }
    },
    {"config",
        {
            {"brokerUrl", "tcp://localhost:1883"},
            {"topicNamespace", "iotea/"}
        }
    }
};
static auto gateway_config = ProtocolGateway::CreateConfig(json{mqtt_config});
static auto gateway = std::make_shared<ProtocolGateway>(gateway_config);
static Client client{gateway};

void signal_handler(int) { client.Stop(); }

int main(int, char**) {
    auto talent = std::make_shared<FunctionProvider>();
    client.RegisterFunctionTalent(talent);

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
