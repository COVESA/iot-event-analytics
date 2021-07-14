##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import json

import xxhash

def json_hash(json_object):
    '''
    Calculates the hash of the stringified version of a json object. The json object is converted into string by sorting
    the keys, removing spaces and new lines.The hash function converts the json string into utf-8 encoding and generates
    a 64-bit code.
    :param json_object: A json object.
    :return: hex string representation of a 64-bit hash.
    '''
    json_str = json.dumps(json_object, indent=None, separators=(',', ':'), ensure_ascii=False, sort_keys=True)
    # the underlying implementation converts json_str to utf-8 byte[]
    return xxhash.xxh64(json_str).hexdigest()
