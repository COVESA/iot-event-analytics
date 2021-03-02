/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/

#include <sys/types.h>
#include <unistd.h>

#include "util.hpp"

#include <cstdlib>
#include <string>
#include <atomic>

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
        srand(static_cast<unsigned int>(getpid()));
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
