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
#include "iotea.hpp"

using json = nlohmann::json;
using namespace iotea::core;

static const char SERVER_ADDRESS[] = "tcp://localhost:1883";
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

static Client client(SERVER_ADDRESS);

void signal_handler(int) { client.Stop(); }

int main(int, char**) {
    auto talent = std::make_shared<FunctionProvider>();
    client.RegisterFunctionTalent(talent);

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
