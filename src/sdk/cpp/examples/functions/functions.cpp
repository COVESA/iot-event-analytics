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
#include <iostream>
#include <memory>

#include "nlohmann/json.hpp"
#include "iotea.hpp"
#include "logging.hpp"
#include "mqtt_client.hpp"


using json = nlohmann::json;
using iotea::core::FunctionTalent;
using iotea::core::Client;
using iotea::core::Callee;
using iotea::core::EventContext;
using iotea::core::CallContext;
using iotea::core::Event;
using iotea::core::IsSet;
using iotea::core::schema::rule_ptr;

namespace logging = iotea::core::log;

constexpr char SERVER_ADDRESS[] = "tcp://localhost:1883";

class MathFunctions : public FunctionTalent {
   private:
    Callee sum;
    Callee fib;

   public:
    MathFunctions()
        : FunctionTalent("math") {
        RegisterFunction("sum", [this](const json& args, CallContext ctx) { Sum(args, ctx); });
        //RegisterFunction("multiply", [this](const json& args, CallContext ctx) { Multiply(args, ctx); });
        //RegisterFunction("fibonnacci", [this](const json& args, CallContext ctx) { Fibonacci(args, ctx); });

        sum = RegisterCallee("math", "sum");
        //fib = RegisterCallee("math", "fibonacci");
    }

    void Sum(const json& args, CallContext& ctx) {
        auto operand = args[0].get<int>();
        logging::Info() << "-> Sum(" << operand << ")";

        if (operand == 1) {
            logging::Info() << "<- 1";
            ctx.Reply(1);
            return;
        }

        logging::Info() << "Call Sum(" << (operand - 1) << ")";
        auto t = ctx.Call(sum, operand - 1);

        ctx.GatherAndReply([operand](std::vector<json> replies) {
            auto s = replies[0].get<int>();

            logging::Info() << "<- " << (operand + s);
            return operand + s;
        }, nullptr, t);
    }

    void Multiply(const json& args, CallContext ctx) {
        ctx.Reply(args[0].get<int>() * args[1].get<int>());
    }

    void Fibonacci(const json& args, CallContext ctx) {
        auto n = args[0].get<int>();

        if (n <= 1) {
            ctx.Reply(n);
            return;
        }

        auto t1 = ctx.Call(fib, n - 1);
        auto t2 = ctx.Call(fib, n - 2);

        ctx.GatherAndReply([](std::vector<json> replies) {
            auto n1 = replies[0].get<int>();
            auto n2 = replies[1].get<int>();

            return n1 + n2;
        }, nullptr, t1, t2);
    }

    void OnEvent(const Event& event, EventContext ctx) override {
        logging::Info() << "Calling function for " << event.GetValue();

        auto t = ctx.Call(sum, event.GetValue());

        ctx.Gather([](const std::vector<json>& replies) {
                logging::Info() << "Result is: " << replies[0].get<int>();
        }, nullptr, t);
    }

    rule_ptr OnGetRules() const override {
        //return IsSet("anyfeature", "anytype");
        return IsSet("temp", "kuehlschrank");
    }
};

static Client client = Client{SERVER_ADDRESS};

void signal_handler(int signal) {
    client.Stop();
}

int main(int argc, char* argv[]) {
    auto talent = std::make_shared<MathFunctions>();
    client.RegisterFunctionTalent(talent);

    std::signal(SIGINT, signal_handler);
    client.Start();

    return 0;
}
