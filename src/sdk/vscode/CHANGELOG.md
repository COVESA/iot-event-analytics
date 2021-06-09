<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Change Log

## [Unreleased]

- 0.9.5 [2021-06-07]
  - Update certificate path within Kuksa.VAL repository

- 0.9.3 [2021-03-09]
  - Add troubleshooting section in README.md
  - Fix version parsing for docker-compose
  - Fix version parsing for Docker using the template format `docker version --format{{.Client.Version}}`

- 0.9.2 [2021-03-09]
  - Fix JSON serialization bug for MQTT Publisher, where empty spaces led to separation of command line arguments

- 0.9.1 [2021-02-23]
  - Display a non-technical error message in case the docker version cannot be retrieved as JSON

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
      - IoT Event Analytics Project root folder _validation_
  - Remove talent class creation command
  - Remove all types definitions to devDependencies to reduce extension size to ~130kB

- 0.8.0 [2021-02-18]
  - Extend configuration to support Python, pip, docker-compose executable and Docker socket (for rootless installations)
  - Extend configuration for IoT Event Analytics platform to make MQTT- and API-ports configurable
  - Extend MQTT Publisher to have specialized inputs for IoTEA values based on feature metadata
  - Fix missing dialog caption

- 0.7.0 [2021-02-12]
  - Add commands to facilitate the integration of Vehicle Signal Specification (VSS) into IoT Event Analytics
    - Create complete Kuksa.val configuration
    - Convert VSS configuration to IoT Event Analytics types configuration

- 0.6.1 [2021-02-10]
  - Rename talent.demo.js to index.js to match package.json
  - Autoselect type and feature on IoT Event Analytics Message type
  - Add commands to start/stop Mosquitto broker instances (To run examples, which only need an MQTT Broker)

- 0.6.0 [2021-02-10]
  - Add MQTT Publisher to VSCode Extension

- 0.5.0
  - Add One-Click Talent project creation using JavaScript SDK
  - Add IoIoT Event AnalyticsTea platform controls
