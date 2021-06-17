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
#include "nlohmann/json.hpp"

#include "client.hpp"
#include "mqtt_client.hpp"

using json = nlohmann::json;
using iotea::core::FunctionTalent;
using iotea::core::Talent;
using iotea::core::Client;
using iotea::core::Callee;
using iotea::core::AndRules;
using iotea::core::IsSet;
using iotea::core::GreaterThan;
using iotea::core::LessThan;
using iotea::core::ErrorMessage;
using iotea::core::Event;
using iotea::core::event_ctx_ptr;
using iotea::core::call_ctx_ptr;
using iotea::core::schema::rule_ptr;

constexpr char SERVER_ADDRESS[] = "tcp://localhost:1883";

class MyService : public FunctionTalent {
   public:
    MyService() : FunctionTalent("my_service") {
        RegisterFunction("multiply", [](const json& args, call_ctx_ptr ctx) {
            auto a = args[0].get<int>();
            auto b = args[1].get<int>();

            ctx->Reply(a * b);
        });
    }

    void OnError(const ErrorMessage& msg) override {
        GetLogger().Error() << "Something went a awry, " << msg.GetMessage(); 
    };
};

class MyReporingTalent : public Talent {
    public:
     MyReporingTalent() : Talent("my_reporting_talent") { }

     void OnEvent(const Event&, event_ctx_ptr) override {
        GetLogger().Info() << "The temp is set!";
     }

     rule_ptr OnGetRules() const override {
        return IsSet("temp", "kuehlschrank");
     }

    void OnError(const ErrorMessage& msg) override {
        GetLogger().Error() << "Something went a awry, " << msg.GetMessage(); 
    };
};

class MyCallingTalent : public Talent {
    private:
        Callee multiply;

    public:
     MyCallingTalent() : Talent("my_calling_talent") {
        multiply = RegisterCallee("my_service", "multiply");
     }

     void OnEvent(const Event& e, event_ctx_ptr ctx) override {
        GetLogger().Info() << "EventReceived in MyCallingTalent";
        auto value = e.GetValue();

        auto token = ctx->Call(multiply, json{value, value}, 1000);

        auto reply_handler = [this](const std::vector<json>& reply) {
            GetLogger().Info() << "kuelschrank.temp=" << reply[0].get<int>(); 
        };
        auto timeout_handler = [this]{
            GetLogger().Info() << "timed out waiting for result";
        };

        ctx->Gather(reply_handler, timeout_handler, token);
     }

     rule_ptr OnGetRules() const override {
        return AndRules(GreaterThan("temp", 2, "kuehlschrank"), LessThan("temp", 10, "kuehlschrank"));
     }

    void OnError(const ErrorMessage& msg) override {
        GetLogger().Error() << "Something went a awry, " << msg.GetMessage(); 
    };
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int) {
    client.Stop();
}

int main(int, char**) {
    client.RegisterFunctionTalent(std::make_shared<MyService>());
    client.RegisterTalent(std::make_shared<MyReporingTalent>());
    client.RegisterTalent(std::make_shared<MyCallingTalent>());

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
