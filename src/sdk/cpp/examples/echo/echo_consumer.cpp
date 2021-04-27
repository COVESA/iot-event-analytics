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
#include <initializer_list>
#include <memory>

#include "nlohmann/json.hpp"
#include "iotea.hpp"
#include "logging.hpp"
#include "mqtt_client.hpp"
#include "schema.hpp"

using namespace iotea::core;
using json = nlohmann::json;

static const std::string SERVER_ADDRESS("tcp://localhost:1883");
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

    void OnEvent(const Event& event, event_ctx_ptr context) override {
        if (event.GetType() == PROVIDED_FETAURE_TYPE) {
            auto message = event.GetValue().get<std::string>();
            log::Info() << "Received message:  '" << message << "'";

            auto t = context->Call(echo_provider.echo, message);

            context->Gather([](std::vector<json> replies) {
                    log::Info() << "Received echo:     '" << replies[0].dump(4) << "'";
                }, nullptr, t);

            log::Info() << "Forwarded message: '" << message << "'";
        } else {
            log::Warn() << "UNKNOWN EVENT RECEIVED";
        }
    }
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int) {
    client.Stop();
}

int main(int, char**) {
    auto talent = std::make_shared<EchoConsumer>();
    client.RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
