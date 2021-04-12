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

const std::string SERVER_ADDRESS("tcp://localhost:1883");

using namespace iotea::core;

static const std::string FEATURE = "provider_talent";
static const std::string FUNC_MULTIPLY = "multiply";
static const std::string FUNC_SUM = "sum";
static const std::string FUNC_FIBONACCI = "fibonacci";

class MathFunctions : public FunctionTalent {
   private:
    Callee fib;

   public:
    explicit MathFunctions()
        : FunctionTalent(FEATURE) {
        RegisterFunction(FUNC_MULTIPLY,
                         [this](const json& args, const CallContext& context) { Multiply(args, context); });

        RegisterFunction(FUNC_FIBONACCI,
                [this](const json& args, const CallContext& context) { Fibonacci(args, context); });

        fib = RegisterCallee(FEATURE, FUNC_FIBONACCI);
    }

    void Multiply(const json& args, const CallContext& context) {
        auto a = args[0].get<int>();
        auto b = args[1];
        auto val = std::to_string(a * b["factor"].get<int>()) + " " + b["unit"].get<std::string>();
        context.Reply(val);

        static int dingdings = 0;
        NewEventContext("my-subject").Emit<int>("dingdings", ++dingdings, "blob");
    }


    void Fibonacci(const json& args, CallContext context) {
        auto n = args[0].get<int>();

        if (n <= 1) {
            context.Reply(n);
            return;
        }

        auto t1 = context.Call(fib, n - 1);
        auto t2 = context.Call(fib, n - 2);

        context.GatherAndReply([](std::vector<json> replies) {
            auto n1 = replies[0].get<int>();
            auto n2 = replies[1].get<int>();

            return n1 + n2;
        }, nullptr, t1, t2);
    }


    schema::rule_ptr OnGetRules() const override { return nullptr; }
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
