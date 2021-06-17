<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics internals

IoT Event Analytics is a broker based complex event processing platform interacting through publish subscribe topics.
To provide the maximum flexibility the work is distributed between the broker and the participating entities.

Broker has to know the

1. __participant group__ and
2. the needed __capability__.

Capability plus participant group allocates the publishing as well as subscribing peers. Possible values are
$share/\<group id\> our /\<group id\>. $share denotes that more than one subscriber is possible aka
broadcast-like communication, in a round-robin manner.

The topic follows a basic schema of

< sharing mode >/< talent id >/< purpose >[/+]

Possible sharing modes are:

1. < blank > , direct broker connection
2. __remote__ , connected via a broker bridging e.g. for failover
3. __ingestion__ , data that is coming from outside the platform and might not follow data schema
4. __configManager__, for discovering purposes

Talent ID is the unique participant identified and purpose is the communication reason

1. __/events__ for passing events (aka signals)
2. __/events/+__ for __distributed__ RPC
3. __/discover__ for discovering other participants

Each passed message/event contains a attribute called __returnTopic__ which denotes where to pass the (__optional__) result to.
