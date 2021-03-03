<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# dapr Adapter

Bosch.IO GmbH
under [MPL-2.0 licence](https://choosealicense.com/licenses/mpl-2.0/)

## Overview

This adapter connects dapr to IoTEA and vice versa.
An overview about the concept behind this adapter can be found here:
https://inside-docupedia.bosch.com/confluence/x/Y5GAVw

## Provided features

At the moment, following features are supported:

- Subscription of a dapr topics (to IoTEA Features) via configuration (./config/pub_sub_mapping.json - in:)
- Subscription of IoTEA features (to dapr topics) via configuration (./config/pub_sub_mapping.json - out:)
- Mapping and handling of dapr request (to IoTEA functions) via configuration (./config/rpc_mapping.json - in:)
- Mapping and handling of iotea request (to dapr functions) via configuration (./config/rpc_mapping.json - out:)

__Important__
For RPC, only content-type="application/json" is currently supported to transfer arguments and receive responses from dapr.

## Pub Sub Mapping

The configuration file can be found here: ./config/pub_sub_mapping.json

### Mapping of dapr topics

The "in"-section contains the mappings of dapr topics to iotea features.

Example:

```json
"in": [
    {
        "dapr": {
            "pubSubName": "pubsub",
            "topic": "vehicle",
            "valueRef": "speed"
        },

        "iotea": {
            "type": null,
            "feature": "speed",
            "description": "Vehicle Speed from dapr",
            "encoding": {
                "type": "number",
                "encoder": null
            },
            "unit": "KMH"
        }
    }
]
```

This example maps the dapr topic "vehicle" with the field (in json-message) "speed" to the IoTEA feature speed with the configured description and encoding. Since topics in dapr are unknown in their structure, these meta data is defined in the mappings.

### Mapping of iotea topics

The "out"-section contains the mappings of IoTEA features to dapr topics.

Example:

```json
"out": [
    {
        "dapr": {
            "pubSubName": "pubsub",
            "topic": "vapp.test_speed"
        },

        "iotea": {
            "type": "default",
            "value_type": "RAW",
            "feature": "subscriber_talent.test_speed",
            "instance": null
        }
    }
]
```

This examples maps the IoTEA feature "subscriber_talent.test_speed" to the dapr topic vapp.test_speed. The feature message is completely send as a payload to dapr topic.

## RPC Mapping

The configuration file can be found here: ./config/rpc_mapping.json

__Important__: Every method name either in dapr or in iotea have to be unique in mappings. If there are multiple providers/types with the same method (e.g. providerA.start() & providerB.start()) then the mapped method must be different (e.g. startA() & startB())

### Mapping of iotea requests

The "out"-section contains the mappings of dapr requests to IoTEA methods.

Example:

```json
{
    "in": [
        {
            "dapr": {
                "methodName": "iotea-echo"
            },

            "iotea": {
                "methodName": "iotea-echo",
                "type": "iotea-rpc_talent",
                "argsBinding": ["iotea_echo", "iotea_name"]
            }
        }

    ]
```

This examples maps the IoTEA method "iotea-rpc_talent.iotea-echo" to the iotea function "dapr_adapter.iotea-echo".

### Mapping of dapr requests

The "out"-section contains the mappings of IoTEA requests to dapr methods.

Example:

```json
    "out": [
        {
            "dapr": {
                "methodName": "dapr-echo",
                "serviceProvider": "dapr-rpc",
                "argsBinding": ["dapr_echo", "dapr_name"],
                "valueRef" : "value"
            },

            "iotea": {
                "methodName": "dapr-echo"
            }

        }
    ]
```

This examples maps the dapr method "dapr-rpc.dapr-echo" to the iotea function "dapr_adapter.dapr-echo".

## OPL / Known Issues

- hbmqtt client has been replaced by paho mqtt client for publish messages. With hmqtt following issues with asyncio occured: If a new loop is used then "got Future <Future pending> attached to a different loop" appeard. If a own loop was used together with asyncio.run_coroutine_threadsafe then rarely an AssertionError occured.
- Dynamical unsubscribe seems to be not possible out of the box without a re-start or perodically updates. (no such methods available)
- All configured dapr subscriptions are defined at startup (static). A more dynamically way is hard to do, because of missing unsubscribe and the unclear payload structures. Dapr itself provides binding mechanism which could be used on dapr side
- For testing purpose there is the possibility to mock dapr_rpc and to test iotea function calls (with hardcoded values):<br>

  ```code
  USE_DAPR_SERVICE_MOCK = True
  TEST_IOTEA_RPC_INTERNAL = True
  ```

## Run the adapter

1. Configure ./config/pub_sub_mapping.json to define the mappings between iotea & dapr
2. Execute _dapr run --app-id python-adapter --app-protocol grpc --log-level error --app-port 50051 python3 dapr_adapter.py_

## Run the examples

The existing config fits the examples in this repo (src/sdk/python/examples/dapr). To run the example, please check the README.md in the example folder.
