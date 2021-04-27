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
#include "iotea.hpp"
#include "logging.hpp"
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
                         [this](const json& args, const CallContext& context) { Echo(args, context); });
        RegisterFunction(FUNC_GET_COUNT,
                         [this](const json& args, const CallContext& context) { GetEchoCount(context); });
        RegisterFunction(FUNC_SET_COUNT,
                         [this](const json& args, const CallContext& context) { SetEchoCount(args, context); });

        int history = 30;
        int ttl = 1000;
        AddOutput(EVENT_ECHO_COUNT, schema::Metadata("Count event triggered by calls to 'echo' function.", history, ttl, "ONE",
                                                     schema::OutputEncoding(schema::OutputEncoding::Type::Number)));
        AddOutput(EVENT_ECHO_RESP_SENT, schema::Metadata("Message event triggered by calls to 'echo' function.", history, ttl, "ONE",
                                                         schema::OutputEncoding(schema::OutputEncoding::Type::String)));
        //SkipCycleCheck(true);
    }

    schema::rule_ptr OnGetRules() const override { return nullptr; }

    void Echo(const json& args, const CallContext& context) {
        auto message = args[0].get<std::string>();
        log::Info() << "Received echo call: " << message;
        ++echoCount_;

        std::transform(message.begin(), message.end(), message.begin(), ::toupper);
        context.Reply(message);
        log::Info() << "Replying echo:      " << message;

        EventContext notifyContext = NewEventContext(NOTIFICATION_CONTEXT);
        notifyContext.Emit(TALENT_NAME+"."+EVENT_ECHO_COUNT, echoCount_);
        notifyContext.Emit(TALENT_NAME+"."+EVENT_ECHO_RESP_SENT, message);
    }

    void GetEchoCount(const CallContext& context) {
        log::Info() << "Received GetEchoCount call";
        context.Reply(echoCount_);
        log::Info() << "Replying echoCount: " << echoCount_;
    }

    void SetEchoCount(const json& args, const CallContext& context) {
        auto newEchoCount = args[0].get<unsigned int>();
        log::Info() << "Received setEchoCount call: " << newEchoCount;
        if (newEchoCount != echoCount_) {
            echoCount_ = newEchoCount;
            EventContext notifyContext = NewEventContext(NOTIFICATION_CONTEXT);
            notifyContext.Emit(TALENT_NAME+"."+EVENT_ECHO_COUNT, echoCount_);
        }
        context.Reply(nullptr);
    }
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int signal) {
    client.Stop();
}

int main(int argc, char* argv[]) {
    auto talent = std::make_shared<EchoProvider>();
    client.RegisterFunctionTalent(talent);

    std::signal(SIGINT, signal_handler);

    client.Start();

    return 0;
}
