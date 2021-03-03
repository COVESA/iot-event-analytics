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
import time

from dapr.clients import DaprClient

with DaprClient() as publisher:
    ID = 0

    while True:
        ID += 1

        MSG_VEHICLE = {
            'id': ID,
            'speed' : ID%100,
            'rpm' : 3000
        }

        MSG_DRIVER = {
            'id': ID,
            'name': 'Max Mustermann'
        }

        publisher.publish_event(
            pubsub_name='pubsub',
            topic='vehicle',
            data=json.dumps(MSG_VEHICLE),
        )

        publisher.publish_event(
            pubsub_name='pubsub',
            topic='driver',
            data=json.dumps(MSG_DRIVER),
        )

        # Print the request
        print('Msg Vehicle: {}'.format(MSG_VEHICLE), flush=True)
        print('Msg Driver: {}'.format(MSG_DRIVER), flush=True)

        #Wait 1s for next publish
        time.sleep(3)
