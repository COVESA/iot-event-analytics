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

from jwcrypto import jwk, jws
from jwcrypto.common import json_encode

import uniresolver

ALLOWED_ALGORITHMS = [
    'HS256', 'HS384', 'HS512',
    'RS256', 'RS384', 'RS512',
    'ES256', 'ES384', 'ES512',
    'PS256', 'PS384', 'PS512',
    'EdDSA', 'ES256K'
]

class Verifier:
    def __init__(self):
        self.resolver = uniresolver.ClientWebResolver('https://resolver.dev.economyofthings.io/1.0/identifiers/')

    def verify(self, jwt_raw):
        web_signature = jws.JWS()
        # pylint: disable=protected-access
        web_signature._allowed_algs = ALLOWED_ALGORITHMS
        web_signature.deserialize(jwt_raw, None, 'ES256K')

        key_id = web_signature.jose_header['kid']
        key_raw = None

        try:
            key_raw = self.resolver.resolveKey(key_id)
        # pylint: disable=broad-except
        except:
            return 'Error resolving key from did'

        if key_raw is None:
            return 'Error resolving key from did'

        key = jwk.JWK.from_json(json.dumps(key_raw))

        try:
            web_signature.verify(key)
            return web_signature.payload
        # pylint: disable=broad-except
        except:
            return 'invalid'

    def signer_did(self, jwt_raw):
        web_signature = jws.JWS()

        # pylint: disable=protected-access
        web_signature._allowed_algs = ALLOWED_ALGORITHMS
        web_signature.deserialize(jwt_raw, None, 'ES256K')
        key_id = web_signature.jose_header['kid']

        split = key_id.split('#')

        if len(split) != 2:
            return 'invalid'

        return split[0]

    def sign(self, claims, key):
        jwsa = jws.JWS(payload=json.dumps(claims))

        # pylint: disable=protected-access
        jwsa._allowed_algs = ALLOWED_ALGORITHMS

        jwsa.add_signature(key, 'ES256K', json_encode({'alg': 'ES256K', 'kid': key.key_id}))

        return jwsa.serialize(compact=True)
