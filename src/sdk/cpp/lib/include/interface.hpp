/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_INTERFACE_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_INTERFACE_HPP_

#include <string>
#include <memory>

namespace iotea {
namespace core {

/**
 * @brief The Receiver interface describes methods required to receive
 * MQTT messages.
 *
 * @param topic The topic the message was sent on
 * @param msg The message payload
 * @param adapter_id The adapter the message was received from
 */
class Receiver {
   public:
    virtual ~Receiver() = default;
    virtual void Receive(const std::string& topic, const std::string& msg, const std::string& adapter_id) = 0;
};

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_INTERFACE_HPP_

