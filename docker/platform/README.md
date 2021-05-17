<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics Platform

The slim build image does not offer any API (Neither Metadata nor Instance API to save ROM and RAM)

## Prerequisites

### >> ARM64 target platform only <<

- Make sure you have Docker 19.03 or above installed and experimental CLI features enable to be able to perform cross platform builds

## Build

- Look into _../../package.json_ in the `<iotea project folder>` to retrieve the `<version>`
- Open the folder `<iotea project folder>` in a terminal
- You can specify the following build arguments:
  - _API\_PORT_ : Adjust the port, which will be exposed, running the API. Has to match the API port in _config.json_ (default=8080)
- __For further information how to build the images (especially, if you are working behind a proxy), please see [here](../README.md)__

### >> ARM64 target platform only <<

- Build your Docker image and export the image as tar-archive<br>
  `docker buildx build --platform linux/arm64 -t iotea-platform-arm64:<version> -o type=oci,dest=./iotea-platform-arm64.<version>.tar -f docker/platform/Dockerfile.arm64 .`
  - To immediately load the image into the local registry, specify the `--load` option instead of `-o...`
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

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=iotea-platform-<version> -v <some folder>:/app/docker/platform/config  iotea-platform-arm64:<version>`

### >> AMD64 target platform only <<

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=iotea-platform-<version> -v <some folder>:/app/docker/platform/config iotea-platform-amd64:<version>`

## Performance

### Node.js options

Add the environment variable `NODE_OPTIONS` with all options, you would like to pass to the platform process. For optimize memory usage, set the parameter `--max-old-space-size` to 4/5 of the available memory for this process in MegaBytes.<br>
Example: `--env NODE_OPTIONS='--max-old-space-size=256' ...`

__Be aware, that starting Node.js with debugger impacts the performance of the platform.__

### Docker options

To reduce the amount of Memory and CPU used, use the flags `--memory=75MB` and `cpus="0.25"`. Be sure that the memory you specify here is calculated via<br>

```javascript
memory = (`max-old-space-size` * 5) / 4 + 10
// https://nodejs.org/docs/latest-v12.x/api/cli.html#cli_useful_v8_options
```

## Debug

- Start your container, and open a shell `docker run -it <see above> /bin/ash` Use `--rm` option to remove the container after stopping it
- Run `node --inspect-brk=0.0.0.0 /app/docker/platform/index.js` from within the container. The platform will wait until you connected to the debugger
- Open your Chrome Browser at page `chrome://inspect/devices`
  - Select your Node.js application and open the debugger
  - Click on the blue arrow to start the IoT Event Analytics application

docker buildx build --platform linux/arm64 -t foo-arm64:0.0.1 --load -f docker/platform/Dockerfile.slim.arm64 .

### >> Linux only <<

- You have to prepend `sudo` to the docker call if you run docker as root (and you are not)
