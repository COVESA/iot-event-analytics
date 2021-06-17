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
#include <memory>
#include <string>

#include "nlohmann/json.hpp"
#include "client.hpp"
#include "mqtt_client.hpp"

using namespace iotea::core;
using json = nlohmann::json;

static const std::string SERVER_ADDRESS("tcp://localhost:1883");
static const std::string TALENT_NAME = "echo_provider";
static const std::string FUNC_ECHO = "echo";
static const std::string FUNC_GET_COUNT = "getEchoCount";
static const std::string FUNC_SET_COUNT = "setEchoCount";
static const std::string NOTIFICATION_CONTEXT = "owner_of_"+TALENT_NAME;
static const std::string EVENT_ECHO_COUNT = "echoCount";
static const std::string EVENT_ECHO_RESP_SENT = "echoResponseSent";

class EchoProvider : public FunctionTalent {
private:
    unsigned int echoCount_{0};

public:
    EchoProvider()
        : FunctionTalent(TALENT_NAME) {
        RegisterFunction(FUNC_ECHO,
                         [this](const json& args, call_ctx_ptr context) { Echo(args, context); });
        RegisterFunction(FUNC_GET_COUNT,
                         [this](const json&, call_ctx_ptr context) { GetEchoCount(context); });
        RegisterFunction(FUNC_SET_COUNT,
                         [this](const json& args, call_ctx_ptr context) { SetEchoCount(args, context); });

        int history = 30;
        int ttl = 1000;
        AddOutput(EVENT_ECHO_COUNT, schema::Metadata("Count event triggered by calls to 'echo' function.", history, ttl, "ONE",
                                                     schema::OutputEncoding(schema::OutputEncoding::Type::Number)));
        AddOutput(EVENT_ECHO_RESP_SENT, schema::Metadata("Message event triggered by calls to 'echo' function.", history, ttl, "ONE",
                                                         schema::OutputEncoding(schema::OutputEncoding::Type::String)));
    }

    schema::rule_ptr OnGetRules() const override { return nullptr; }

    void Echo(const json& args, call_ctx_ptr context) {
        GetLogger().Info() << "Raw args: " << args.dump(4);
        auto message = args[0].get<std::string>();
        GetLogger().Info() << "Received echo call: " << message;
        ++echoCount_;

        std::transform(message.begin(), message.end(), message.begin(), ::toupper);
        context->Reply(message);
        GetLogger().Info() << "Replying echo: " << message;

        auto notifyContext = NewEventContext(NOTIFICATION_CONTEXT);
        notifyContext->Emit(TALENT_NAME+"."+EVENT_ECHO_COUNT, echoCount_);
        notifyContext->Emit(TALENT_NAME+"."+EVENT_ECHO_RESP_SENT, message);
    }

    void GetEchoCount(call_ctx_ptr context) {
        GetLogger().Info() << "Received GetEchoCount call";
        context->Reply(echoCount_);
        GetLogger().Info() << "Replying echoCount: " << echoCount_;
    }

    void SetEchoCount(const json& args, call_ctx_ptr context) {
        auto newEchoCount = args[0].get<unsigned int>();
        GetLogger().Info() << "Received setEchoCount call: " << newEchoCount;
        if (newEchoCount != echoCount_) {
            echoCount_ = newEchoCount;
            auto notifyContext = NewEventContext(NOTIFICATION_CONTEXT);
            notifyContext->Emit(TALENT_NAME+"."+EVENT_ECHO_COUNT, echoCount_);
        }
        context->Reply(nullptr);
    }
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int) {
    client.Stop();
}

int main(int, char**) {
    auto talent = std::make_shared<EchoProvider>();
    client.RegisterFunctionTalent(talent);

    std::signal(SIGINT, signal_handler);

    client.Start();

    return 0;
}
