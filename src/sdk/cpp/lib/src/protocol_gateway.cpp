/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/
#include "dlfcn.h"

#include <algorithm>
#include <fstream>
#include <iostream>

#include "logging.hpp"
#include "protocol_gateway.hpp"

namespace iotea {
namespace core {

///////////////////
// PubSubOptions //
///////////////////
PubSubOptions::PubSubOptions(bool platform_proto_only, const std::string& adapter_id)
    : platform_proto_only_{platform_proto_only}
    , adapter_id_{adapter_id} {

}

bool PubSubOptions::IsPlatformProtoOnly() const {
    return platform_proto_only_;
}

std::string PubSubOptions::GetAdapterId() const {
    return adapter_id_;
}

////////////////////
// PublishOptions //
////////////////////
PublishOptions::PublishOptions(bool platform_proto_only, const std::string& adapter_id)
    : PubSubOptions{platform_proto_only, adapter_id}
    , retain_{false}
    , stash_{true} {}

bool PublishOptions::Retain() const {
    return retain_;
}

bool PublishOptions::Stash() const {
    return stash_;
}

//////////////////////
// SubscribeOptions //
//////////////////////
SubscribeOptions::SubscribeOptions(bool platform_proto_only, const std::string& adapter_id)
    : PubSubOptions{platform_proto_only, adapter_id} {}

/////////////
// Adapter //
/////////////
Adapter::Adapter(const std::string& name, bool is_platform_proto)
    : name_{name}
    , is_platform_proto_{is_platform_proto} {}

std::string Adapter::GetName() const {
    return name_;
}

bool Adapter::IsPlatformProto() const {
    return is_platform_proto_;
}

//////////////////////////////
// ProtocolGatewayException //
//////////////////////////////
ProtocolGatewayException::ProtocolGatewayException(const std::string& msg, const Code& code)
    : msg_{msg}
    , code_{code} {}

const char* ProtocolGatewayException::what() const noexcept {
    return msg_.c_str();
}

ProtocolGatewayException::Code ProtocolGatewayException::GetCode() const noexcept {
    return code_;
}

/////////////////////
// ProtocolGateway //
/////////////////////
static auto logger = iotea::core::logging::NamedLogger("ProtocolGateway");

const PublishOptions ProtocolGateway::DefaultPublishOptions{false, ""};

const SubscribeOptions ProtocolGateway::DefaultSubscribeOptions{false, ""};

void ProtocolGateway::ValidateConfig(const json& config, bool platform_proto_only) {
    logger.Debug() << "ProtocolGateway config: \n" << config.dump(2);
    auto adapters = config.find("adapters");
    if (adapters == config.end()) {
        throw ProtocolGatewayException(R"(Invalid ProtocolGateway configuration. Field "adapters" is missing.)", ProtocolGatewayException::Code::INVALID_CONFIGURATION);
    }

    if (!adapters->is_array()) {
        throw ProtocolGatewayException(R"(Invalid ProtocolGateway configuration. Field "adapters" must be an array.)", ProtocolGatewayException::Code::INVALID_CONFIGURATION);
    }

    if (adapters->size() == 0) {
        throw ProtocolGatewayException(R"(Invalid ProtocolGateway configuration. Field "adapters" must contain at least one adapter.)", ProtocolGatewayException::Code::INVALID_CONFIGURATION);
    }

    // Verify that at most one platform adapter is configured
    auto platform_adapter_count = 0;
    for (const auto& adapter : *adapters) {
        if (adapter.find("platform") != adapter.end()) {
            platform_adapter_count += adapter["platform"].get<bool>() ? 1: 0;
            if (platform_adapter_count > 1) {
                throw ProtocolGatewayException(R"(Invalid ProtocolGateway configuration. More than one platform adapter found.)", ProtocolGatewayException::Code::INVALID_CONFIGURATION);
            }
        }
    }

    if (platform_proto_only && platform_adapter_count == 0)
    {
        throw ProtocolGatewayException(R"(Invalid ProtocolGateway configuration. Should use platform protocol only but not platform adapter found.)", ProtocolGatewayException::Code::INVALID_CONFIGURATION);
    }
}

json ProtocolGateway::CreateConfig(json adaptor_configs) {
    return json{
        {"adapters", adaptor_configs}
    };
}

ProtocolGateway::ProtocolGateway(const json& config, const std::string& display_name, bool platform_proto_only)
    : config_(config)
    , platform_proto_only_{platform_proto_only} {
    ValidateConfig(config, platform_proto_only);
    (void)display_name;
}

void ProtocolGateway::Initialize() {
    logger.Debug() << "Loading adapters";
    for (const auto& c : config_["adapters"]) {
        auto is_platform_proto = c["platform"].get<bool>();
        auto module_name = c["module"]["name"].get<std::string>();
        auto module_config = c["config"];

        logger.Debug() << "Loading adapter from " << module_name;

        auto handle = dlopen(module_name.c_str(), RTLD_LAZY);
        if (handle == nullptr) {
            auto msg = std::string{"Failed to load plugin  "} + module_name;
            throw ProtocolGatewayException(msg, ProtocolGatewayException::Code::PLUGIN_LOAD_FAILURE);
        }

        auto load = (std::shared_ptr<Adapter> (*)(const std::string&, bool, const json&))dlsym(handle, "Load");
        if (load == nullptr) {
            auto msg = std::string{R"(Failed to lookup symbol "Load" in ")"} + module_name;
            throw ProtocolGatewayException(msg, ProtocolGatewayException::Code::PLUGIN_SYM_NOT_FOUND);
        }

        auto adapter = load(module_name, is_platform_proto, module_config);
        adapters_.push_back(adapter);

        // Modules are never unloaded so we intentionally never call dlclose()
    }
}

void ProtocolGateway::Start() {
    std::for_each(adapters_.begin(), adapters_.end(), [](std::shared_ptr<Adapter> adapter) {
        adapter->Start();
    });
}

void ProtocolGateway::Stop() {
    std::for_each(adapters_.begin(), adapters_.end(), [](std::shared_ptr<Adapter> adapter) {
        adapter->Stop();
    });
}

bool ProtocolGateway::IsValidOperation(std::shared_ptr<Adapter> adapter, const PubSubOptions& opts) const {
    auto platform_proto_only = &opts == &DefaultPublishOptions ? platform_proto_only_ : opts.IsPlatformProtoOnly();
    auto id = opts.GetAdapterId();
    return (!platform_proto_only || adapter->IsPlatformProto()) &&
           (id.empty() || id == adapter->GetName());
}

void ProtocolGateway::Publish(const std::string& topic, const std::string& msg, const PublishOptions& opts) {
    std::for_each(adapters_.begin(), adapters_.end(), [&](std::shared_ptr<Adapter> adapter) {
        if (IsValidOperation(adapter, opts)) {
            adapter->Publish(topic, msg, opts);
        }
    });
}

void ProtocolGateway::Subscribe(const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) {
    std::for_each(adapters_.begin(), adapters_.end(), [&](std::shared_ptr<Adapter> adapter) {
        if (IsValidOperation(adapter, opts)) {
            adapter->Subscribe(topic, on_msg, opts);
        }
    });
}

void ProtocolGateway::SubscribeShared(const std::string& group, const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) {
    std::for_each(adapters_.begin(), adapters_.end(), [&](std::shared_ptr<Adapter> adapter) {
        if (IsValidOperation(adapter, opts)) {
            adapter->SubscribeShared(group, topic, on_msg, opts);
        }
    });
}

} // core
} // iotea
