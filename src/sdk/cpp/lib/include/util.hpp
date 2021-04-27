/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef IOTEA_UTIL_HPP
#define IOTEA_UTIL_HPP

#include <string>

namespace iotea {
namespace core {

std::string GetEnv(const std::string& name, const std::string& defval = "");
int64_t GetEpochTimeMs();

std::string GenerateUUID();

}  // namespace core
}  // namespace iotea

#endif  // IOTEA_UTIL_HPP
