<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Agent - Network example

## Overview

This example shows the verification process between a charging station and a car.

The charging API provides the following methods:

- getVIN
- getCredential
- postOfferAccept
- postTransactionEnd

The VerifyTalent provides _verify_ for verification.

There are three talents involved:

1. ChargeApiTalent (javascript) which provides the corresponding API and acts as the vehicle
2. ChargingstationTalent which acts as the Charging station and uses the ChargeAPI and handles a transaction
3. VerifyTalent which is used to verify requests

The sequence is:

1. _ChargeRequest_ is received by ChargeApiTalent
2. ChargeApiTalent send a _ChargeOffer_
3. _ChargeOffer_ is received by ChargingStationTalent and starts the transaction
3.1. - GetCrendential()
3.2. - Verify()
3.3. - PostOfferAccept()
3.4. - PostTransactionEnd()

## Prerequisites

Install required libraries for the VerifyTalent: _pip install -r src/sdk/python/examples/integrations/agent-network/requirements.txt_

### Issue with cffi_backend

If you have faced this issue:

> from cryptography.hazmat.bindings._padding import lib
> ModuleNotFoundError: No module named '_cffi_backend'"

then

1. Uninstall cryptography: _pip uninstall cryptography_
2. Install cffi: _pip install cffi_
3. Install cryptography: _pip install cryptography_

## Run the example

You have to start all three talents:

1. ChargeAPI: _node src/sdk/javascript/examples/integrations/agent-network/index.js_
2. ChargingStationTalent: _python src/sdk/python/examples/integrations/agent-network/charging/run.py_
3. VerifyTalent: _python src/sdk/python/examples/integrations/agent-network/verify/run.py_

Send the trigger event ChargeApi.chargeRequest which will start the sequence and will print out logs for more information.
