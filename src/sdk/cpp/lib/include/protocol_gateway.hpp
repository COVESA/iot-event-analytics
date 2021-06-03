/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_PROTOCOL_GATEWAY_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_PROTOCOL_GATEWAY_HPP_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace iotea {
namespace core {

/**
 * @brief
 *
 */
class PubSubOptions {
   public:
    PubSubOptions(bool platform_proto_only, const std::string& adapter_id);

    virtual ~PubSubOptions() = default;

    virtual bool IsPlatformProtoOnly() const;

    virtual std::string GetAdapterId() const;

   protected:
    bool platform_proto_only_;
    std::string adapter_id_;
};

/**
 * @brief
 *
 */
class PublishOptions : public PubSubOptions {
   public:
    PublishOptions(bool platform_proto_only, const std::string& adapter_id);

    virtual ~PublishOptions() = default;

    virtual bool Retain() const;

    virtual bool Stash() const;

   private:
    bool retain_;
    bool stash_;
};

/**
 * @brief
 *
 */
class SubscribeOptions : public PubSubOptions {
   public:
    SubscribeOptions(bool platform_proto_only, const std::string& adapter_id);

    virtual ~SubscribeOptions() = default;
};

using on_msg_func_ptr = std::function<void(
    const std::string&, // topic
    const std::string&, // message
    const std::string&)>; // adapter id

/**
 * @brief
 *
 */
class Adapter {
   public:
    Adapter(const std::string& name, bool is_platform_proto);

    virtual ~Adapter() = default;

    virtual std::string GetName() const;

    virtual bool IsPlatformProto() const;

    virtual void Start() {};

    virtual void Stop() {};

    virtual void Publish(const std::string& topic, const std::string& msg, const PublishOptions& opts) = 0;

    virtual void Subscribe(const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) = 0;

    virtual void SubscribeShared(const std::string& group, const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) = 0;

   protected:
    std::string name_;
    bool is_platform_proto_;
};

/**
 * @brief
 */
class ProtocolGatewayException : public std::exception {
   public:
       enum class Code {
           INVALID_CONFIGURATION,
           PLUGIN_LOAD_FAILURE,
           PLUGIN_SYM_NOT_FOUND,
           READ_CONFIG_FAILURE,
       };

       ProtocolGatewayException(const std::string& msg, const Code& code);

       virtual ~ProtocolGatewayException() = default;

       virtual const char* what() const noexcept;

       virtual Code GetCode() const noexcept;

   private:
    std::string msg_;
    Code code_;
};

/**
 * @brief
 *
 */
class ProtocolGateway {
   private:
    static const PublishOptions DefaultPublishOptions;
    static const SubscribeOptions DefaultSubscribeOptions;

   public:
    ProtocolGateway(const json& config, const std::string& display_name = "", bool platform_proto_only = false);

    virtual ~ProtocolGateway() = default;

    virtual void Publish(const std::string& topic, const std::string& msg, const PublishOptions& options = DefaultPublishOptions);

    virtual void Subscribe(const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts = DefaultSubscribeOptions);

    virtual void SubscribeShared(const std::string& group, const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts = DefaultSubscribeOptions);

    virtual void Initialize();
    virtual void Start();
    virtual void Stop();

    static void ValidateConfig(const json& config, bool platform_proto_only);
    static json CreateConfig(json adaptor_configs);

   private:
    bool IsValidOperation(std::shared_ptr<Adapter> adapter, const PubSubOptions& opts) const;

    json config_;
    bool platform_proto_only_;
    std::vector<std::shared_ptr<Adapter>> adapters_;
};

using gateway_ptr = std::shared_ptr<ProtocolGateway>;

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_PROTOCOL_GATEWAY_HPP_

