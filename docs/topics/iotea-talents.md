<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics Talents

Talents differ between the respective security domains and programming, but follow a common schema. Each Talent just has to define its required data points / services `method [addOutput]` and the provided data points / services `method [getRules]` automatically notified `method [onEvent]` if the demanded data is available.

Due to the fact that everything follows a non-blocking channeling ([compare](https://www.enterpriseintegrationpatterns.com/patterns/messaging/MessageChannel.html)) IO concept the service invocation follows a request-reply ([compare](https://www.enterpriseintegrationpatterns.com/patterns/messaging/RequestReply.html)), not a request-response integration ([compare](https://www.enterpriseintegrationpatterns.com/patterns/messaging/EncapsulatedSynchronousIntegration.html)), style. The used programming model is __map-worker-reduce__, where a reduce method performs the summary operation. This reduce method can be in the same (process) context, but not mandatory need to.

This allows the runtime to

- orchestrate the processing through distributed environments e.g. in vehicle and cloud,
- run the various tasks in parallel,
- manage all communications and data transfers between the various parts of the system e.g. managing off- and online situations,
- and provide for redundancy and fault tolerance.

Required data points are defined by so called (s)ignal (t)emporal (l)ogic (STL) rules. In its simplest case represents Boolean rules of _"when the data is given"_. Temporal logic is a kind of qualifying data points terms of __time__ - temporal logic always has the ability to reason about a timeline. For data points time exist in four aspects

- __needed history__ e.g. the last 10 values are always required on an update
- __value encoding__ e.g. if an update happens the encoded value should represent the delta from the last value
- how long a data point value/ event keeps alive in the STL logic – the so called __time-to-live (TTL)__ – after it happens
- how the values are __guarded__.

The safeguard covers:

- (u)nit (o)f (m)easurement conversions
- ingestion-encoding-routing channel
- and per default activated  call-cycle check and resolution
- graceful degradation.

All these safeguard happens on the data point (aka feature) level. Also compare the [feature](FEATURE_README.md) description.

The following JSON schema describes what is possible: [feature.schema.json](https://github.com/GENIVI/iot-event-analytics/resources/feature.schema.json)

__Note:__ the channeling is parallel to the data point type system.  In some programming contexts these channels are also called ETL-pipelines or data type channel ([compare](https://www.enterpriseintegrationpatterns.com/patterns/messaging/DatatypeChannel.html)).

Talent examples are given for

- NodeJS here: [NodeJS SDK](https://github.com/GENIVI/iot-event-analytics/src/sdk/javascript/examples)
- Python >3.6 here [Python SDK](https://github.com/GENIVI/iot-event-analytics/src/sdk/python)

__Important to know:__

- One central default talent is the so called `ConfigManager`. The `ConfigManager` supports the communication between talents and manages basic runtime topic including the __type-system__, __uom-system__ and inter talent __discovery__. Also compare the [IoT Event Analytics components README](./iotea-components.md).
- Do not miss to set the MQTT_TOPIC_NS environment variable e.g. in Python:

  ```code
  import os
  os.environ['MQTT_TOPIC_NS']='iotea/'
  ```
