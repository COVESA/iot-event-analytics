##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from dapr.ext.grpc import App, InvokeServiceRequest, InvokeServiceResponse

APP = App()

@APP.method(name='dapr-echo')
def mymethod(request: InvokeServiceRequest) -> InvokeServiceResponse:
    print(request.metadata, flush=True)
    print(request.text(), flush=True)

    return InvokeServiceResponse('Hello from Dapr! Arguments are ignored', 'application/json')

APP.run(50080)
