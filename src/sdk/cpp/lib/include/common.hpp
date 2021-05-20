/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_COMMON_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_COMMON_HPP_

#include <string>
#include <functional>

namespace iotea {
namespace core {

static constexpr char MQTT_TOPIC_NS[] = "iotea";
static constexpr char DEFAULT_INSTANCE[] = "default";
static constexpr char DEFAULT_TYPE[] = "default";

using uuid_generator_func_ptr = std::function<std::string(void)>;

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_COMMON_HPP_
