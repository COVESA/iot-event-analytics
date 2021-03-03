<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics Platform

## Prerequisites

### >> ARM64 target platform only <<

- Make sure you have Docker 19.03 or above installed and experimental CLI features enable to be able to perform cross platform builds

## Build

- Clone the boschio.iotea project from [here](https://github.com/GENIVI/iot-event-analytics)
- Go into the folder, into which you cloned the project. This folder will be referred to as `<iotea project folder>`
- Look into the _package.json_ file in the `<iotea project folder>` to retrieve the `<version>`

### >> AMD64 target platform only <<

- You can specify the following build arguments:
  - __>> AMD64 target platform only <<__<br>
    _HTTP\_PROXY_ and _HTTPS\_PROXY_: Specify these variables if the internet is only reachable via a proxy. (optional)
  - _API\_PORT_ : Adjust the port, which will be exposed, running the API. Has to match the API port in _config.json_ (default=8080)

### >> ARM64 target platform only <<

- Build your Docker image and export the image as tar-archive<br>
  `docker buildx build --platform linux/arm64 -t iotea-platform-arm64:<version> -o type=oci,dest=./iotea-platform-arm64.<version>.tar -f docker/platform/Dockerfile.arm64 .`
- Import this image
  - `sudo docker load --input iotea-platform-arm64.<version>.tar`
  - `sudo docker tag <SHA256-Hash> iotea-platform-arm64:<version>`

### >> AMD64 target platform only <<

- Build your Docker image using the local registry<br>
  `docker build -t iotea-platform-amd64:<version> -f docker/platform/Dockerfile.amd64 .`

## Install

- Create a folder to store the configuration `<some folder>`
- The minimal directory structure should look like this. Can be copied from _./config_<br>

  ```code
  <some-folder>
  |- channels
  |  |- talent.channel.json
  |  L- talent.schema.json
  |- config.json
  |- uom.json
  L- types.json
  ```

- Edit the files to match your setup

## Run

### >> ARM64 target platform only <<

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=iotea-platform-<version> -v <some folder>:/home/node/app/docker/platform/config  iotea-platform-arm64:<version>`

### >> AMD64 target platform only <<

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=iotea-platform-<version> -v <some folder>:/app/docker/platform/config iotea-platform-amd64:<version>`

### >> Linux only <<

- You have to prepend `sudo` to the docker call if you run docker as root (and you are not)