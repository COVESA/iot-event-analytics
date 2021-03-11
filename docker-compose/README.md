<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Usage of Docker Compose

## Overview

This folder contains docker-compose configurations (*.yml) for mqtt and platform which builds all needed containers (build) and to run it (up).

## Prerequisites

- Tested with
  - Windows
    - Docker desktop v2.4.0.0
    - Docker Engine v19.03.13
    - Compose: 1.27.4
  - Ubuntu 18.04.5
    - Docker Engine v19.03.6
    - Compose 1.27.4

## Setup custom configuration

If you want to create a custom configuration, create a config folder at _\<YOUR CONFIG PATH\>_, and copy the following files/folders into it

- ./_mosquitto_
- ./_platform_ (Only needs to be copied, if you want to use _docker-compose.platform.yml_)
- ./.env

Adapt the configuration files as needed and update the `PLATFORM_CONFIG_DIR` and the `MOSQUITTO_CONFIG_DIR` variable within the copied _.env_ file

- _PLATFORM_CONFIG_DIR_ needs to point to _\<YOUR CONFIG PATH\>/platform_
- _MQTT_CONFIG_DIR_ needs to point to _\<YOUR CONFIG PATH\>/mosquitto_

__The paths within the _.env_ file need to be relative to the _docker-compose_ folder__

### (Optional) Linux px proxy configuration

If you are running behind a px-proxy on Linux (e.g. using [Bosch Open Source desktop](https://inside-docupedia.bosch.com/confluence/x/nRujEQ)) you need to ensure the binding between your docker network and proxy is configured.

1.) __~/.px/config.ini:__ Ensure binding ___"binds = [...], \<docker network proxy\>___ (e.g. 172.17.0.1:3128) exists

`[server]
binds = 127.0.0.1:3128, 172.17.0.1:3128#`

If not, add it and restart your proxy (e.g. via _osd-proxy-restart_ for osd)

To check that the binding exists you can call for your proxy port (e.g. 3128):
`netstat -ntlpn | grep -i 3128`

Which should show the your docker-network proxy (e.g. 172.17.0.1:3128):
`tcp       0     0 172.17.0.1:3128        0.0.0.0:*              LISTEN     12391/python3`

2.) __~/.docker/config.json:__ Ensure _http(s)Proxys_ in your docker-network have the same port as your host proxy (e.g. [http://172.17.0.1:3128])

```json
{
 "proxies":
 {
   "default":
   {
     "httpProxy": "http://172.17.0.1:3128",
     "httpsProxy": "http://172.17.0.1:3128"
   }
 }
}
```

3.) __/etc/systemd/system/docker.service.d/http_proxy.conf__: Ensure that the http(s)_proxies are set

```code
[Service]
Environment=HTTP_PROXY=http://localhost:3128/
Environment=HTTPS_PROXY=http://localhost:3128/
```

Afterwards you have to restart your docker daemon:
`sudo systemctl daemon-reload`
`sudo systemctl restart docker`

To check your env-variables for docker you can call:
`sudo systemctl show --property=Environment docker`

## Setup

- If you are behind a corporate proxy, update DOCKER_HTTP_PROXY and DOCKER_HTTPS_PROXY in the _.env_ file. __If NOT, remove these lines completly from the .env file__
  - __>> Windows only: <<__ Use `docker.for.win.localhost` to refer to your computer i.e. _[http://docker.for.win.localhost:3128](http://docker.for.win.localhost:3128)_ assuming your proxy is running locally on Port 3128
  - __>> Linux only: <<__ Use _[http://172.17.0.1:3128](http://172.17.0.1:3128)_ as proxy address

## Build & Run (from within the _./docker-compose_ directory)

Platform containers can be build with this command: \
```docker-compose -f docker-compose.platform.yml build```

Platform containers can be run with this command: \
```docker-compose -f docker-compose.platform.yml up```

Mosquitto MQTT broker containers (local and local-remote) can be run and built with this command: \
```docker-compose -f docker-compose.mosquitto.yml up --build```

## Build & Run (from within the _./docker-compose_ directory with custom environment configuration)

```docker-compose -f docker-compose.mosquitto.yml --env-file <YOUR CONFIG PATH>/.env up --build```

## Debug

- Install the Docker Extension for VSCode
  - As an alternative without using VSCode, use the Chrome debugger and open the URL [chrome://inspect/#devices](chrome://inspect/#devices) in your Browser and select the matching Target
- Go to the debugging view and click on the `gears` icon. This brings up the launch.json file
  - Click on `Add Configuration...` and select `Docker: Attach to node` from the list
  - Create the following configurations<br>

  ```json
  {
      "type": "node",
      "request": "attach",
      "protocol": "inspector",
      "name": "Attach to IoT Event Analytics Pipeline Docker Container",
      "port": 9229,
      "address": "localhost",
      "localRoot": "${workspaceFolder}",
      "remoteRoot": "/app"
  },
  {
      "type": "node",
      "request": "attach",
      "protocol": "inspector",
      "name": "Attach to IoT Event Analytics ConfigManager Docker Container",
      "port": 9230,
      "address": "localhost",
      "localRoot": "${workspaceFolder}",
      "remoteRoot": "/app"
  }
  ```

- Start the containers using `docker-compose -f docker-compose.platform.yml -f docker-compose.platform.debug.yml --env-file <YOUR CONFIG PATH>/.env up --build`<br>
  This will start NodeJS with inspect-brk option using default Port _9229_ for the pipeline and _9230_ for the ConfigManager.
- Go to the debug view again and select one of the above configurations from the list to attach the debugger to the container. Now you are able to use e.g. breakpoints directly in the source code

## Hints

If you run the platform containers then please ensure that there is no other MQTT broker (e.g. mosquitto) is running on the same machine on localhost with the same port
