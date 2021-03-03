##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import requests

class ClientWebResolver:
    def __init__(self, uri):
        if not uri.endswith('/'):
            uri += '/'

        self.uri = uri
        self._lookup = {}

    def resolve(self, did):
        url = self.uri + did
        http_response = requests.get(url, timeout=5)

        if http_response.status_code == 200:
            return http_response.json()

        return

    def resolve_key(self, didkey):
        if didkey in self._lookup:
            return self._lookup[didkey]

        split = didkey.split('#')

        if len(split) != 2:
            return

        did = split[0]
        key_id = split[1]
        doc = self.resolve(did)
        keys = doc['didDocument']['publicKey']

        for key_entry in keys:
            if (key_entry['id'] == key_id) or (key_entry['id'] == didkey):
                key = key_entry['publicKeyJwk']

                self._lookup[didkey] = key

                return key
