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
#include <initializer_list>
#include <memory>

#include "nlohmann/json.hpp"
#include "client.hpp"
#include "protocol_gateway.hpp"

using namespace iotea::core;
using json = nlohmann::json;

static const std::string TALENT_NAME("echo_consumer");
static const std::string PROVIDED_FEATURE_NAME("messageString");
static const std::string PROVIDED_FETAURE_TYPE(schema::DEFAULT_TYPE);
static const std::string CALLED_TALENT_NAME("echo_provider");
static const std::string CALLED_METHOD_NAME("echo");


class EchoConsumer : public Talent {
   private:
    struct EchoProvider {
        Callee echo;
    } echo_provider;

   public:
    EchoConsumer()
        : Talent(TALENT_NAME) {

        int ttl = 1000;
        int history = 30;
        AddOutput(PROVIDED_FEATURE_NAME, schema::Metadata("Message to be forwarded to echo provider", history, ttl, "ONE",
                                                          schema::OutputEncoding(schema::OutputEncoding::Type::String)));

        echo_provider.echo = RegisterCallee(CALLED_TALENT_NAME, CALLED_METHOD_NAME);
        schema_.SkipCycleCheckFor({PROVIDED_FETAURE_TYPE+"."+TALENT_NAME+"."+PROVIDED_FEATURE_NAME});
    }

    schema::rule_ptr OnGetRules() const override {
        return IsSet(TALENT_NAME+"."+PROVIDED_FEATURE_NAME);
    }

    void OnEvent(event_ptr event, event_ctx_ptr context) override {
        if (event->GetType() == PROVIDED_FETAURE_TYPE) {
            auto message = event->GetValue().get<std::string>();
            GetLogger().Info() << "Received message:  '" << message << "'";

            auto t = context->Call(echo_provider.echo, message);

            context->Gather([this](std::vector<json> replies) {
                    GetLogger().Info() << "Received echo:     '" << replies[0].dump(4) << "'";
                }, nullptr, t);

            GetLogger().Info() << "Forwarded message: '" << message << "'";
        } else {
            GetLogger().Warn() << "UNKNOWN EVENT RECEIVED";
        }
    }
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

    auto talent = std::make_shared<EchoConsumer>();
    client->RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);
    client->Start();

    return 0;
}
