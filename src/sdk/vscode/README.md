<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics VSCode Extension

A Visual Studio Code extension providing scaffolding and code completion for developing __Talents__ for IoT Event Analytics.

IoT Event Analytics is a freely scalable event processing and agent network platform. The platform is build around __Talents__ which form a distributed network. Each talent is a unit of work and can interact with other Talents via events. The platform itself orchestrates the flow of events and offers additional services, like an in-memory digital twin, calculation of statistical data based on given event histories and complex rule evaluation of when to inform a Talent, that something happened, that the Talent "is interested in".

Thus, a talent is always an event consumer and may also be an event producer. The event-driven business logic can be implemented using one of our SDKs for JavaScript, Python or C++.

# Getting Started

- Install the extension: Open the _Extensions_ navigator by pressing `Ctrl-Shift-X`, search for `iotea`, select the `IoT Event Analytics` extension and click _Install_ 
- Get and install IoT Event Analytics platform: See [https://github.com/genivi/iot-event-analytics/](https://github.com/genivi/iot-event-analytics/)

## Commands
Press `Ctrl-Shift-P` on Windows/Linux or `⇧ + ⌘ + P` on Mac to open the command palette. Enter `iotea` to see all available commands:
- Starting and stopping the IoTEA platform
- Publishing IoTEA Events to the MQTT broker
- Creating new Talent projects

## Code Completion

_Note: "Trigger suggest" can be triggered using `Ctrl-Space` on Windows/Linux or for MacOS, use `⌥ + Esc` on Apple Keyboards and `Alt + Esc` on Windows Keyboards._

Use the following pattern to trigger the auto completion when creating new rules and constraints in the `getRules()` block of __Talents__:

- `iotea.rule.and` + _"Trigger suggest"_ > Create new And Rule
- `iotea.rule.or` + _"Trigger suggest"_ > Create new Or Rule
- `iotea.rule.op` + _"Trigger suggest"_ > Create a new Rule with an Operation Constraint
- `iotea.rule.change` + _"Trigger suggest"_ > Create a new Rule with a Change Constraint

# Configuration

## Requirements

Following requirements have to be matched to use this Extension

- Docker >=19.3.13
- docker-compose >= 1.27.4
- Node.js >=12.3.0
- git (CLI) _any version_
- python >= 3.6.8
- pip _any version_

Additionally, all requirements will be checked on the first command invocation automatically.

## Manual Installation

When you build from sources by following the build steps in the main readme, install the VSCode extensions with the following steps:
- In IoTEA root project folder: `yarn sdk.build`
- Go to _View_ > _Extensions_ > _..._ > _Install from VSIX_
- Select _lib/iotea.vsix_
- Restart Visual Studio Code

## Settings

- Go to _File_ > _Preferences_ > _Settings_ and type _iotea_ to specify machine-wide settings
- Open extension page to get an overview over all contribution points and available commands

| Setting | Default | Description |
|---------|---------|-------------|
| Platform API Endpoint | `http://localhost:8080/metadata/api/v1` | Metadata API Endpoint (port has to match given Platform API port) |
| Platform MQTT Endpoint | `mqtt://localhost:1883` | MQTT Endpoint (port has to match given MQTT port) |
| Platform MQTT Port | `1883` | MQTT Port |
| Platform API Port | `8080` | Platform API port for REST Swagger UI |
| Project Root Directory | (empty) | IoT Event Analytics project root folder |
| Project Docker Proxy | (empty) | The http(s) proxy used during creation of the platforms docker images (e.g. http://host.docker.internal:3128) |
| VSS Path Separator | `.` (Dot) | Path separator for VSS paths |
| VSS Path Replacer | `.` -> `$` | Character replacements from VSS path to IoT Event Analytics features |
| Terminal Docker-Sock | (empty) | Has to be set for rootless docker e.g. unix:///run/user/1001/docker.sock |
| Terminal Docker | `docker` | Command to start docker |
| Terminal Docker-Compose | `docker-compose` | Command to start docker-compose |
| Terminal Python | `python3` | Command to start Python 3 |
| Terminal Pip | `pip` | Pip module |
| Autocomplete Types Features Refresh Interval | `10000` | Refresh interval for types and features for autocompletion | 

## Troubleshooting

- _"I cannot select an .env file on macOS file open dialog"_<br>
  These files might be hidden from view. Just hit `⌘ + ⇧ + .` to show hidden files in the dialog.
- _"The Docker version check fails"_<br>
  Make sure you have the latest Docker version installed. The docker version is retrieved using: `docker version --format={{.Client.Version}}`. Test in your console, if this command returns the version number for your current docker installation.
- _"The Python version check fails"_<br>
  You can specify the Python interpreter and the pip module in the settings. It defaults to `python3` and `pip` for standalone installations. If you are running a virtual environment setup based on Anaconda, you have to use `python` and `pip`. On Linux-Systems it can be different. Please check in a terminal, which command to use and update the settings accordingly.
- _"I cannot publish MQTT messages"_<br>
  Make sure you executed `yarn` in the IoT Event Analytics project folder to install all needed dependencies
