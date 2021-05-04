/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <chrono>
#include <random>
#include <iomanip>
#include <string>
#include <sstream>

#include "util.hpp"

namespace iotea {
namespace core {

std::string GetEnv(const std::string& name, const std::string& defval) {
    auto v = std::getenv(name.c_str());
    return v ? v : defval;
}

int64_t GetEpochTimeMs() {
    auto epoch = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
}

Uuid4::Uuid4() noexcept {
    Generate();
    Stringify();
}

Uuid4::operator std::string() const noexcept {
    return str_;
}

void Uuid4::Generate() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, 255);

    // From RFC4122:
    //
    // 4.4.  Algorithms for Creating a UUID from Truly Random or
    // Pseudo-Random Numbers
    //
    // The version 4 UUID is meant for generating UUIDs from truly-random or
    // pseudo-random numbers.
    //
    // The algorithm is as follows:
    //
    // o  Set the two most significant bits (bits 6 and 7) of the
    //    clock_seq_hi_and_reserved to zero and one, respectively.
    //
    // o  Set the four most significant bits (bits 12 through 15) of the
    //    time_hi_and_version field to the 4-bit version number from
    //    Section 4.1.3.
    //
    // o  Set all the other bits to randomly (or pseudo-randomly) chosen
    //    values.

    for (int i = 0; i < 16; i++) {
        bits_[i] = dist(gen);
    }

    // Field                  Data Type     Octet  Note
    // clock_seq_hi_and_rese  unsigned 8    8      The high field of the
    //                                             clock sequence
    //                                             multiplexed with the
    //                                             variant
    bits_[7] &= 0xbf; // Clear bit 6
    bits_[7] |= 0x80; // Set bit 7

    // 4.1.3.  Version
    //
    // The version number is in the most significant 4 bits of the time
    // stamp (bits 4 through 7 of the time_hi_and_version field).
    //
    // The following table lists the currently-defined versions for this
    // UUID variant.
    //
    // Msb0  Msb1  Msb2  Msb3   Version  Description
    //  0     1     0     0        4     The randomly or pseudo-
    //                                   randomly generated version
    //                                   specified in this document.

    // Field                  Data Type     Octet  Note
    // time_hi_and_version    unsigned 16   6-7    The high field of the
    //                                             bit integer
    //                                             timestamp multiplexed
    //                                             with the version number
    bits_[5] &= 0x4f; // Clear bits 7, 5 and 4
    bits_[5] |= 0x40; // Set bit 6
}

void Uuid4::Stringify() {
    static constexpr char chars[] = "0123456789abcdef";
    static constexpr int dash_indices[] = {4, 6, 8, 10};
    const int* didx = dash_indices;

    std::stringstream ss;

    for (int i = 0; i < 16; i++) {
        if (i == *didx) {
            ss << "-";
            didx++;
        }

        auto octet = bits_[i];
        ss << chars[octet >> 4] << chars[octet & 0x0f];
    }

    str_ = ss.str();
}

std::string GenerateUUID() {
    return Uuid4();
}

}  // namespace core
}  // namespace iotea
