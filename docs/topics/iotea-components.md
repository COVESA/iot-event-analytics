<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics Platform

## Features/Components

- Can be deployed in the cloud (e.g. Kubernetes, Any nodeJS capable runtime, via Docker-compose)
- Can be deployed on the edge device itself --> Traffic to cloud (for cloud talents) is reduced to a minimum by using smart message routing via mosquitto

### Config-Manager

- Sends out periodically discovery messages to all talents
- Validates discovery data using JSON Schema
- Sends rules updates to all Rule-Managers
- Holds Metadata-Manager master instance which is the single master loading metadata for static segments/types and features
- Sets indices for incoming dynamic features from talents
- Updates all Metadata-Manager worker instances via MQTT
- __Rules-Manager__
  - Holds internal rule state (for all talents)
  - Synchronized via MQTT by Config-Manager

### Metadata-Manager Worker

- Holds:
  - Definition of Segments
  - Definition of types inheriting all features from a given segment
    - Inherited features can be overridden (except the index field)
    - Synchronized via MQTT by Config-Manager

### Instance-Manager

- Keeps track of all instances of a given subject (An instance is a specific device of a specific type belonging to a specific segment)
- Sets and retrieves features from instances (Keeps track of previous features for delta encoding)
- Synchronizes its internal state via MQTT with all other Instance-Manager instances
- TTL for every field to simulate a rolling time window

### Ingestion

- Transforms and validates incoming events, configurable as ETL-Pipeline
- Multi-Channel supports an arbitary amout of paralell ETL-Pipelines
- Completly configurable via config files (JSONata transformations) and JSON Schema validations
- Validates value type against metadata (boolean, string, number, object, any)
- Validates type against metadata using Metadata-Manager Slave
- Freely scalable by ingestion instance count

### Encoding

- Encodes incoming events by various encoders (minmax, delta, categorical) --> Not implemented yet
- Uses Metadata-Manager Slave to retrieve metadata
- Uses Instance-Manager to store an encoded field into the appropriate instance
- Freely scalable by ingestion instance count

### Routing

- Freely scalable by ingestion instance count
- Uses rules to determine whether an event should be forwareded to a specific talent
  - Rules can be any type of boolean algebra (containing and/or rules)
  - Enriches event with all fields a talent is "interested" in

### Talent

- Supports local and cloud talents (using FAAS)
  - Automatically syncs to cloud message broker via mosquitto bridging
- Encapsulates business logic
- Programming language agnostic (Only JSON processing required)
- Map-Reduce support by using Mapper, Workers and Reducer
- FunctionTalents mimic RPC based on streaming events
