##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from cloudevents.sdk.event import v1
from dapr.ext.grpc import App

APP = App()
@APP.subscribe(pubsub_name='pubsub', topic='vapp.anyfeature')
def mytopic(event: v1.Event) -> None:
    print(event.Data(), flush=True)

print(App.__mro__)

APP.run(50060)
