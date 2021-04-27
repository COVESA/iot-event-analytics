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

using json = nlohmann::json;

const std::string SERVER_ADDRESS("tcp://localhost:1883");

using namespace iotea::core;

class EventConsumer : public Talent {
   private:
    struct ProviderTalent {
        Callee Multiply;
        Callee Fib;
    } provider_talent;

   public:
    EventConsumer()
        : Talent("event_consumer") {
        provider_talent.Multiply = RegisterCallee("provider_talent", "multiply");
        provider_talent.Fib = RegisterCallee("provider_talent", "fibonacci");
    }

    void OnEvent(const Event& event, event_ctx_ptr context) override {
        if (event.GetType() == "kuehlschrank") {
            auto args =
                json{event.GetValue().get<int>(), json{{"factor", event.GetValue().get<int>()}, {"unit", "thing"}}};

            auto t = context->Call(provider_talent.Multiply, args);

            context->Gather([](std::vector<json> replies) {
                log::Info() << "Multiply result: " << replies[0].dump(4);
            }, nullptr, t);

            auto s = context->Call(provider_talent.Fib, args, 100);

            auto handle_result = [](std::vector<json> replies) {
                log::Info() << "Fibonacci result: " << replies[0].dump(4);
            };
            auto handle_timeout = [](){
                log::Info() << "******* Timed out waiting for result";
            };

            context->Gather(handle_result, handle_timeout, s);
        } else if (event.GetType() == "blob") {
            log::Info() << "Currently at " << event.GetValue().dump() << " dingdings";
        }
    }

    schema::rule_ptr OnGetRules() const override {
        return OrRules(AndRules(GreaterThan("temp", 2, "kuehlschrank"), LessThan("temp", 10, "kuehlschrank")),
                        IsSet("dingdings", "blob"));
    }
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int) {
    client.Stop();
}

int main(int, char**) {
    auto talent = std::make_shared<EventConsumer>();
    client.RegisterTalent(talent);

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
