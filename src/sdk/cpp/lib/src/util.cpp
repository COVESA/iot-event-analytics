/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <string>
#include <atomic>

#include "util.hpp"

namespace iotea {
namespace core {

std::string GetEnv(const std::string& name, const std::string& defval) {
    auto v = std::getenv(name.c_str());
    return v ? v : defval;
}

// TODO implement proper UUID generator
std::string GenerateUUID() {
    std::string uuid = "";
    static const int seglens[] = {8, 4, 4, 4, 12};
    static const auto chars = std::string{"0123456789abcdef"};
    static std::atomic_flag flag = ATOMIC_FLAG_INIT;

    if (!flag.test_and_set()) {
        auto seed =  static_cast<unsigned int>(getpid());
        srand(seed);
    }

    for (size_t s = 0; s < sizeof(seglens) / sizeof(seglens[0]); s++) {
        if (s > 0) {
            uuid += '-';
        }

        for (int i = 0; i < seglens[s]; i++) {
            uuid += chars[rand() % chars.size()];
        }
    }

    return uuid;
}

}  // namespace core
}  // namespace iotea
