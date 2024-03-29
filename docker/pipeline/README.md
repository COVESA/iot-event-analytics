<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics Pipeline

## Prerequisites

### >> ARM64 target platform only <<

- Make sure you have Docker 19.03 or above installed and experimental CLI features enable to be able to perform cross platform builds

## Build

- Look into _../../package.json_ in the `<iotea project folder>` to retrieve the `<version>`
- Open the folder `<iotea project folder>` in a terminal
- __For further information how to build the images (especially, if you are working behind a proxy), please see [here](../README.md)__

### >> ARM64 target platform only <<

- Build your Docker image and export the image as tar-archive<br>
  `docker buildx build --platform linux/arm64 -t iotea-pipeline-arm64:<version> -o type=oci,dest=./iotea-pipeline-arm64.<version>.tar -f docker/pipeline/Dockerfile.arm64 .`
- Import this image
  - `sudo docker load --input iotea-pipeline-arm64.<version>.tar`
  - `sudo docker tag <SHA256-Hash> iotea-pipeline-arm64:<version>`

### >> AMD64 target platform only <<

- Build your Docker image using the local registry<br>
  `docker build -t iotea-pipeline-amd64:<version> -f docker/pipeline/Dockerfile.amd64 .`

## Install

- Create a folder to store the configuration `<some folder>`
- The minimal directory structure should look like this. Can be copied from _./config_<br>

  ```code
  <some-folder>
  |- channels
  |  |- talent.channel.json
  |  L- talent.schema.json
  L- config.json
  ```

- Edit the files to match your setup

## Run

### >> ARM64 target platform only <<

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=iotea-pipeline-<version> -v <some folder>:/home/node/app/docker/pipeline/config  iotea-pipeline-arm64:<version>`

### >> AMD64 target platform only <<

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=iotea-pipeline-<version> -v <some folder>:/app/docker/pipeline/config iotea-pipeline-amd64:<version>`

### >> Linux only <<

- You have to prepend `sudo` to the docker call if you run docker as root (and you are not)
