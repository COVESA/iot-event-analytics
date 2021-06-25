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
 * @brief PubSubOptions is the base class for options provided to the
 * ProtocolGateway::Subscribe, ProtocolGateway::SubscribeShared and
 * ProtocolGateway::Publish methods.
 *
 */
class PubSubOptions {
   public:
    /**
     * @brief Construct a new PubSubOptions.
     *
     * @param platform_proto_only Toggle whether to apply option only to
     * platform protocol adapters.
     * @param adapter_id The id of the adapter to apply the option to or and
     * empty string to apply the option to all adapters.
     */
    PubSubOptions(bool platform_proto_only, const std::string& adapter_id = "");

    virtual ~PubSubOptions() = default;

    /**
     * @brief Get the "is platform only" status of this option.
     *
     * @return bool
     */
    virtual bool IsPlatformProtoOnly() const;

    /**
     * @brief Get the ID of the adapter configured for this option.
     *
     * @return std::string
     */
    virtual std::string GetAdapterId() const;

    /**
     * @brief Compare this PubSubOptions to another.
     *
     * @param other The other PubSubOptions.
     * @return bool
     */
    virtual bool operator==(const PubSubOptions& other) const;

   protected:
    bool platform_proto_only_;
    std::string adapter_id_;
};

/**
 * @brief PublishOptions describes options to apply to the
 * ProtocolGateway::Publish method.
 *
 */
class PublishOptions : public PubSubOptions {
   public:
    /**
     * @brief Construct a new PublishOptions.
     *
     * @param platform_proto_only Toggle whether to apply option only to
     * platform protocol adapters.
     * @param adapter_id The id of the adapter to apply the option to or and
     * empty string to apply the option to all adapters.
     */
    PublishOptions(bool platform_proto_only, const std::string& adapter_id);

    virtual ~PublishOptions() = default;

    /**
     * @brief Get the retain status of this option.
     *
     * @return bool
     */
    virtual bool Retain() const;

    /**
     * @brief Get the stash status of this option.
     *
     * @return bool
     */
    virtual bool Stash() const;

    /**
     * @brief Compare this PublishOptions to another.
     *
     * @param other The other PublishOptions.
     * @return bool
     */
    virtual bool operator==(const PublishOptions& other) const;

   private:
    bool retain_;
    bool stash_;
};

/**
 * @brief SubscribeOptions describes options to apply to the
 * ProtocolGateway::Subscribe and ProtocolGateway::SubscribeShared methods.
 *
 */
class SubscribeOptions : public PubSubOptions {
   public:
    /**
     * @brief Constructs a new SubscribeOptions
     *
     * @param platform_proto_only Toggle whether to apply option only to
     * platform protocol adapters.
     * @param adapter_id The id of the adapter to apply the option to or and
     * empty string to apply the option to all adapters.
     */
    SubscribeOptions(bool platform_proto_only, const std::string& adapter_id);

    virtual ~SubscribeOptions() = default;

    /**
     * @brief Compare this SubscribeOptions to another.
     *
     * @param other The other SubscribeOptions.
     * @return bool
     */
    virtual bool operator==(const SubscribeOptions& other) const;
};

using on_msg_func_ptr = std::function<void(
    const std::string&, // topic
    const std::string&, // message
    const std::string&)>; // adapter id

/**
 * @brief Adapter describes the interface of protocol adapters.
 */
class Adapter {
   public:
    /**
     * @brief Constructs a new Adapter.
     *
     * @param name The name of the Adapter.
     * @param is_platform_proto Describe whether the Adapter implements a
     * platform protocol.
     */
    Adapter(const std::string& name, bool is_platform_proto);

    virtual ~Adapter() = default;

    /**
     * @brief Get the name of the Adapter.
     *
     * @return std::string
     */
    virtual std::string GetName() const;

    /**
     * @brief Get the platform protocol status of the Adapter.
     *
     * @return bool
     */
    virtual bool IsPlatformProto() const;

    /**
     * @brief Start the Adapter.
     */
    virtual void Start() {};

    /**
     * @brief Stop the Adapter.
     */
    virtual void Stop() {};

    /**
     * @brief Publish a message to the Adapter.
     *
     * @param topic The topic to publish to.
     * @param msg The message to publish.
     * @param opts Options to pass along to the Adapter.
     */
    virtual void Publish(const std::string& topic, const std::string& msg, const PublishOptions& opts) = 0;

    /**
     * @brief Subscribe to messages from the Adapter
     *
     * @param topic The topic to subscribe to.
     * @param on_msg_func_ptr A callback function to call when a message
     * matching the subcription arrives.
     * @param opts Options to pass along to the Adapter.
     */
    virtual void Subscribe(const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) = 0;

    /**
     * @brief Subscribe to shared messages from the Adapter
     *
     * @param topic The topic to subscribe to.
     * @param on_msg_func_ptr A callback function to call when a message
     * matching the subcription arrives.
     * @param opts Options to pass along to the Adapter.
     */
    virtual void SubscribeShared(const std::string& group, const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts) = 0;

   protected:
    std::string name_;
    bool is_platform_proto_;
};

/**
 * @brief ProtocolGatewayException describes errors encounterd by the ProtocolGateway.
 */
class ProtocolGatewayException : public std::exception {
   public:
       enum class Code {
           INVALID_CONFIGURATION,
           PLUGIN_LOAD_FAILURE,
           PLUGIN_SYM_NOT_FOUND,
           READ_CONFIG_FAILURE,
       };

       /**
        * @brief Constructs a new ProtocolGatewayException.
        *
        * @param msg A descriptive message descripting the error.
        * @param code The error code that best matches the error.
        */
       ProtocolGatewayException(const std::string& msg, const Code& code);

       virtual ~ProtocolGatewayException() = default;

       /**
        * @brief Get a description of the error.
        *
        * @return const char*
        */
       virtual const char* what() const noexcept;

       /**
        * @brief Get the code associated with error.
        *
        * @return ProtocolGatewayException::Code
        */
       virtual Code GetCode() const noexcept;

   private:
    std::string msg_;
    Code code_;
};

/**
 * @brief The ProtocolGateway is an abstraction layer between the applications
 * and the transport protocol. It dynamically loads one or more Adapters that
 * provide the actual transport.
 *
 */
class ProtocolGateway {
   private:
    static const PublishOptions DefaultPublishOptions;
    static const SubscribeOptions DefaultSubscribeOptions;

   public:
    /**
     * @brief Constructs a new ProtocolGateway.
     *
     * @param config A JSON representation of the configuration.
     * @param display_name A name forwarded to each Adapter.
     * @param platform_proto_only Controls whether to load only platform protocol Adapters.
     */
    ProtocolGateway(const json& config, const std::string& display_name = "", bool platform_proto_only = false);

    virtual ~ProtocolGateway() = default;

    /**
     * @brief Publish a message to the Adapters.
     * ProtocolGateway::Start() must be called before
     * ProtocolGateway::Publish().
     *
     * @param topic The topic to publish to.
     * @param msg The message to publish.
     * @param opts Options to pass along to the Adapters.
     */
    virtual void Publish(const std::string& topic, const std::string& msg, const PublishOptions& options = DefaultPublishOptions);

    /**
     * @brief Subscribe to messages from the Adapters.
     * ProtocolGateway::Start() must be called before
     * ProtocolGateway::Subscribe().
     *
     * @param topic The topic to subscribe to.
     * @param on_msg_func_ptr A callback function to call when a message
     * matching the subcription arrives.
     * @param opts Options to pass along to the Adapters.
     */
    virtual void Subscribe(const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts = DefaultSubscribeOptions);

    /**
     * @brief Subscribe to shared messages from the Adapters.
     * ProtocolGateway::Start() must be called before
     * ProtocolGateway::SubscribeShared().
     *
     * @param topic The topic to subscribe to.
     * @param on_msg_func_ptr A callback function to call when a message
     * matching the subcription arrives.
     * @param opts Options to pass along to the Adapters.
     */
    virtual void SubscribeShared(const std::string& group, const std::string& topic, on_msg_func_ptr on_msg, const SubscribeOptions& opts = DefaultSubscribeOptions);

    /**
     * @brief Initialize the ProtocolGateway by loading all the Adapters listed
     * in the configuration. Throws ProtocolGatewayException if any of the
     * Adapters fail to load.
     */
    virtual void Initialize();

    /**
     * @brief Start the ProtocolGateway. ProtocolGateway::Initialize() must be
     * called before ProtocolGateway::Start().
     */
    virtual void Start();

    /**
     * @brief Stop the ProtocolGateway.  ProtocolGateway::Start() must be
     * called before ProtocolGateway::Stop().
     */
    virtual void Stop();

    /**
     * @brief Validate a configuration file. Throws ProtocolGatewayException if the configuration is invalid.
     */
    static void ValidateConfig(const json& config, bool platform_proto_only);

    /**
     * @brief Create a basic configuration.
     *
     * @return json
     */
    static json CreateConfig(json adaptor_configs);

   protected:
    /**
     * @brief A contructor for tests only.
     */
    ProtocolGateway(const std::string& display_name, bool platform_proto_only);

    /**
     * @brief Add an adapter to the set of adapters.
     *
     * @param adatper The new Adapter.
     *
     * @return bool
     */
    virtual bool Add(std::shared_ptr<Adapter> adapter);


   private:
    bool IsValidOperation(std::shared_ptr<Adapter> adapter, const PubSubOptions& opts) const;

    json config_;
    std::string display_name_;
    bool platform_proto_only_;
    std::vector<std::shared_ptr<Adapter>> adapters_;
};

using gateway_ptr = std::shared_ptr<ProtocolGateway>;

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_PROTOCOL_GATEWAY_HPP_

