<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Mosquitto MQTT Broker

## Prerequisites

### >> ARM64 target platform only <<

- Make sure you have Docker 19.03 or above installed and experimental CLI features enable to be able to perform cross platform builds

## Build

- Open the folder `<iotea project folder>/docker/mosquitto` in a terminal
- `<version>` will reference the version of the base image in the Dockerfile (`Dockerfile.*64`)
- You can specify the following build arguments:
  - _WS\_PORT_: Port, which will be exposed for Websocket interface
  - _MQTT\_PORT_: Port, which will be exposed for MQTT Interface
- __For further information how to build the images (especially, if you are working behind a proxy), please see [here](../README.md)__

### >> ARM64 target platform only <<

- Build your Docker image and export the image as tar-archive<br>
  `docker buildx build --platform linux/arm64 -t iotea-mosquitto-arm64:<version> -o type=oci,dest=./iotea-mosquitto-arm64.<version>.tar -f Dockerfile.arm64 .`
- Import this image
  - `sudo docker load --input iotea-mosquitto-arm64.<version>.tar`
  - `sudo docker tag <SHA256-Hash> iotea-mosquitto-arm64:<version>`

### >> AMD64 target platform only <<

- `docker build -t iotea-mosquitto-amd64:<version> -f Dockerfile.amd64 .`

## Install

- Create a folder to store the configuration `<some folder>`
- Copy the configuration file from `<iotea project folder>/docker/mosquitto/config.json` into `<some folder>` and edit it to fit your needs<br>
  The directory structure should look like this<br>

  ```code
  <some folder>
  L- config.json
  ```

## Run

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" --restart=unless-stopped --name=mosquitto -d=true -v <some folder>:/mosquitto/config iotea-mosquitto-<arch>:<version>`

### >> Linux only <<

- You have to prepend `sudo` to the docker call if you run docker as root (and you are not)

## Good to know

- If you want to start an MQTT broker on another port, simply run the following
  - `docker run -p=1883:1884 -d=true --restart unless-stopped --name=mosquitto-remote eclipse-mosquitto:latest`
