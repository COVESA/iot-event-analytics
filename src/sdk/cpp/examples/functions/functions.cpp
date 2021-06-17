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
#include <memory>

#include "nlohmann/json.hpp"
#include "client.hpp"
#include "protocol_gateway.hpp"


using json = nlohmann::json;
using iotea::core::ProtocolGateway;
using iotea::core::FunctionTalent;
using iotea::core::Client;
using iotea::core::Callee;
using iotea::core::call_ctx_ptr;
using iotea::core::event_ptr;
using iotea::core::event_ctx_ptr;
using iotea::core::IsSet;
using iotea::core::schema::rule_ptr;


class MathFunctions : public FunctionTalent {
   private:
    Callee sum;
    Callee fac;
    Callee fib;

   public:
    MathFunctions()
        : FunctionTalent("math") {
        RegisterFunction("sum", [this](const json& args, call_ctx_ptr ctx) { Sum(args, ctx); });
        RegisterFunction("factorial", [this](const json& args, call_ctx_ptr ctx) { Factorial(args, ctx); });
        RegisterFunction("fibonacci", [this](const json& args, call_ctx_ptr ctx) { Fibonacci(args, ctx); });

        sum = RegisterCallee("math", "sum");
        fac = RegisterCallee("math", "factorial");
        fib = RegisterCallee("math", "fibonacci");
    }

    void Sum(const json& args, call_ctx_ptr ctx) {
        auto operand = args[0].get<int>();

        if (operand == 1) {
            ctx->Reply(1);
            return;
        }

        auto t = ctx->Call(sum, operand - 1);

        ctx->GatherAndReply([operand](std::vector<json> replies) {
            auto s = replies[0].get<int>();

            return operand + s;
        }, nullptr, t);
    }

    void Factorial(const json& args, call_ctx_ptr ctx) {
        auto n = args[0].get<int>();

        if (n == 1) {
            ctx->Reply(1);
            return;
        }

        auto t = ctx->Call(fac, n - 1);

        ctx->GatherAndReply([n](std::vector<json> replies) {
            auto m = replies[0].get<int>();

            return n * m;
        }, nullptr, t);
    }

    void Fibonacci(const json& args, call_ctx_ptr ctx) {
        auto n = args[0].get<int>();

        if (n <= 1) {
            ctx->Reply(n);
            return;
        }

        auto t1 = ctx->Call(fib, n - 1);
        auto t2 = ctx->Call(fib, n - 2);

        ctx->GatherAndReply([](std::vector<json> replies) {
            auto n1 = replies[0].get<int>();
            auto n2 = replies[1].get<int>();

            return n1 + n2;
        }, nullptr, t1, t2);
    }

    void OnEvent(event_ptr event, event_ctx_ptr ctx) override {
        auto v = event->GetValue();

        auto tsum = ctx->Call(sum, v);
        ctx->Gather([this, v](const std::vector<json>& replies) {
                GetLogger().Info() << "sum(" << v << ") = " << replies[0].get<int>();
        }, nullptr, tsum);

        auto tfac = ctx->Call(fac, v);
        ctx->Gather([this, v](const std::vector<json>& replies) {
                GetLogger().Info() << "fac(" << v << ") = " << replies[0].get<int>();
        }, nullptr, tfac);

        auto tfib = ctx->Call(fib, v);
        ctx->Gather([this, v](const std::vector<json>& replies) {
                GetLogger().Info() << "fib(" << v << ") = " << replies[0].get<int>();
        }, nullptr, tfib);
    }

    rule_ptr OnGetRules() const override {
        return IsSet("anyfeature", "anytype");
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

    client->RegisterFunctionTalent(std::make_shared<MathFunctions>());

    std::signal(SIGINT, signal_handler);
    client->Start();

    return 0;
}
