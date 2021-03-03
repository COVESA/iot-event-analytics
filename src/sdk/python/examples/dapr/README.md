<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# dapr Example

## Overview

This examples shows pub-sub and rpc handling via dapr_adapter.

The publisher periodically sends two topics:

- vehicle with speed and rpm as payload (which is mapped to the IoTEA features speed and rpm)
- driver with name as payload (which is mapped to IoT Event Analytics feature driver)

The subscriber-talent subscribes the IoTEA feature and prints them if received. When it receives the feature it also calls _dapr-echo_ which is provided by invoke_-_receiver. (RPC call not tested with invoke_receiver, implemented via mock!) If tests iotea rpc tests are enable (see README.md for dapr_adapter) then iotea_rpc_talent will be called.

The subscriber subscribes the dapr topic _vapp.anytype_ and prints the event if arrived (can be send (ingestion) in IoTEA via cli (see below))

## Steps

0. Run the IoTEA Platform (e.g. via docker-compose)
1. Run the dapr\_adapter from _src/adapter/dapr_:

   ```code
   dapr run --app-id python-adapter --app-protocol grpc --log-level error --app-port 50051 python3 dapr_adapter.py
   ```

   (Configuration for this example is already done, but maybe need to adapted)
2. Run the iotea_rpc_talent:

   ```code
   python3 iotea_rpc_talent.py
   ```

3. Run the invoke_receiver:

   ```code
   dapr run --app-id dapr-rpc --app-protocol grpc --app-port 50080 python3 invoke_receiver.py
   ```

4. Run the publisher with dapr:

   ```code
   dapr run --app-id python-publisher --app-protocol grpc python3 publisher.py
   ```

5. Run the subscriber talent:

   ```code
   python3 subscriber_talent
   ```

6. Run the subscriber:

   ```code
   dapr run --app-id python-subscriber --app-protocol grpc --log-level error --app-port 50060 python3 subscriber.py
   ```

7. Optional: Ingestion feature (iotea) message via:

   ```code
   node cli.js pub -c "mqtt://localhost:1883" -t "iotea/ingestion/events" -f "../../sdk/javascript/examples/dapr/events.txt" --times 1 --delayMs 2000`
   ```

__Info__: After discovery the behavior shall be:

1. publisher periodically send topic messages to IoTEA
2. Topic messages from publisher are received by dapr_adapter which maps to the corresponding IoTEA feature and sends them out
3. IoTEA features are received by subscriber-talent which will trigger a request (_dapr-echo_) via dapr_adapter to invoke_receiver which responds to the subscriber_talent (this can be mocked in dapr_adapter, have a look the the README.md in its directory)
4. (If IoTEA RPC Tests enabled in dapr_adapter (see README.md in its direcotry)): dapr_adapter will trigger _iotea-rpc.iotea-echo_ call.
