<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Change Log

## [Unreleased]

- 0.9.0 [2021-02-19]
  - Add configurable platform prechecks before any command is executed<br>
    The checks are done once. When successfully done, the state is stored in the configuration, until the requirements change
    - The checks contain
      - Docker Engine _version_
      - Docker _proxy configuration_
      - docker-compose _version_
      - NodeJS _version_
      - git _any version_
      - python _version_
      - pip _any version_
      - IoTea Project root folder _validation_
  - Remove talent class creation command
  - Remove all types definitions to devDependencies to reduce extension size to ~130kB

- 0.8.0 [2021-02-18]
  - Extend configuration to support Python, pip, docker-compose executable and Docker socket (for rootless installations)
  - Extend configuration for IoT Event Analytics platform to make MQTT- and API-ports configurable
  - Extend MQTT Publisher to have specialized inputs for IoTEA values based on feature metadata
  - Fix missing dialog caption

- 0.7.0 [2021-02-12]
  - Add commands to facilitate the integration of Vehicle Signal Specification (VSS) into IoT Event Analytics
    - Create complete Kuksa.VAL configuration
    - Convert VSS configuration to IoT Event Analytics types configuration

- 0.6.1 [2021-02-10]
  - Rename talent.demo.js to index.js to match package.json
  - Autoselect type and feature on IoT Event Analytics Message type
  - Add commands to start/stop Mosquitto broker instances (To run examples, which only need an MQTT Broker)

- 0.6.0 [2021-02-10]
  - Add MQTT Publisher to VSCode Extension

- 0.5.0
  - Add One-Click Talent project creation using JavaScript SDK
  - Add IoTea platform controls
