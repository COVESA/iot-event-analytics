<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Kuksa.val2IoTEventAnalytics Adapter

## Prerequisites

### >> ARM64 target platform only <<

- Make sure you have Docker 19.03 or above installed and experimental CLI features enable to be able to perform cross platform builds

__IMPORTANT:__ If you have a running installation of __Kuksa.val__ you can skip the following Install/Run Kuksa.val instructions.

### Install Kuksa.val

- Open [https://kuksaval.northeurope.cloudapp.azure.com/job/kuksaval-upstream/job/master](https://kuksaval.northeurope.cloudapp.azure.com/job/kuksaval-upstream/job/master) in your browser

### >> ARM64 target platform only <<

- Download file _kuksa-val-arm64.tar.xz_

### >> AMD64 target platform only <<

- Download file _kuksa-val-amd64.tar.xz_

- Load the image `docker image load --input kuksa-val-<arch>.tar.xz`<br>
  The image should automatically have been tagged `<image tag>` in the newest releases
- If the image is not tagged, it can be done by `docker tag <SHA256 Hash of image> <image tag>`

- Create a configuration directory. It will be referenced by `<some folder>` and should look like this

  ```code
  <some folder>
  |- certs
  |  |- jwt.key.pub
  |  |- Server.key
  |  L- Server.pem
  L- vss.json
  ```

- Directory contents
  - Build GENIVI Reference JSON model **_vss.json_** from vspec as described [https://github.com/GENIVI/vehicle_signal_specification](https://github.com/GENIVI/vehicle_signal_specification/tree/master) using the provided vss-tools
    - Clone the repository into _vehicle\_signal\_specification_
    - Open _vehicle\_signal\_specification/vss-tools_
    - Install all Python dependencies by running `pip install -r requirements.txt --user`
    - Run `python vspec2json.py -i Vehicle:vehicle.uuid ../spec/VehicleSignalSpecification.vspec vss.json`
    - Copy _vss.json_ into your confiruation directory `<some folder>`
    - Modify the _vss.json_ file

      ```json
      {
        "Vehicle": {
          "Driver": {
            "Identifier": {
              "Subject": {
                "description": "Subject for the authentification of the occupant. E.g. UserID 7331677",
                "datatype": "string",
                "type": "sensor",
                "uuid": "...",
                "value": "ishouldbetheuserid"                         // Add a default value for the Subject
              }
            }
          }
        }
      }

      {
        "Vehicle": {
          "VehicleIdentification": {
            "VIN": {
              "description": "17-character Vehicle Identification Number (VIN) as defined by ISO 3779",
              "datatype": "string",
              "type": "attribute",
              "uuid": "...",
              "value": "ishouldbeavin"                                // Add a default value for the VIN
            }
          }
        }
      }
      ```

  - The contents of the _certs_ folder can be downloaded from:
    - [jwt.key.pub](https://raw.githubusercontent.com/eclipse/kuksa.val/master/certificates/jwt/jwt.key.pub)
    - [Server.key](https://raw.githubusercontent.com/eclipse/kuksa.val/master/certificates/Server.key)
    - [Server.pem](https://raw.githubusercontent.com/eclipse/kuksa.val/master/certificates/Server.pem)
  - __Make sure to have the latest certificates matching your installation. You might have to check the commit from the last built of the server.__

- Make sure to copy the `<JSON-Web Token>` from [here](https://github.com/eclipse/kuksa.val/blob/master/certificates/jwt/super-admin.json.token)<br>
  Make sure you update this token in your _kuksa.val2iotea configuration file_ and in any other configuration file, which needs to authenticate against the Kuksa.val server.
  - If you have to create a new token (e.g. because the old one expired), execute `python createToken.py super-admin.json` to create a newly signed token based on the configuration found in _super-admin.json_

### Run Kuksa.val

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" --restart=unless-stopped -d=true --name=kuksa.val -v <some folder>:/config/ -e KUKSAVAL_OPTARGS=--insecure -e LOG_LEVEL=ALL <image tag>`<br>
  The insecure argument enables non-encrypted communication with the server over the websocket protocol

## Build

- Look into _../../package.json_ in the `<iotea project folder>` to retrieve the `<version>`
- Open the folder `<iotea project folder>` in a terminal
- __For further information how to build the images (especially, if you are working behind a proxy), please see [here](../README.md)__

### >> ARM64 target platform only <<

- Build your Docker image and export the image as tar-archive<br>
  `docker buildx build --platform linux/arm64 -t kuksa.val2iotea-adapter-arm64:<version> -o type=oci,dest=./kuksa.val2iotea-adapter-arm64.<version>.tar -f docker/kuksa.val2iotea/Dockerfile.arm64 .`
- Import this image
  - `sudo docker load --input iotea-platform-arm64.<version>.tar`
  - `sudo docker tag <SHA256-Hash> iotea-platform-arm64:<version>`

### >> AMD64 target platform only <<

- Build your Docker image using the local registry<br>
  `docker build -t kuksa.val2iotea-adapter-amd64:<version> -f docker/kuksa.val2iotea/Dockerfile.amd64 .`

## Install

- Create a folder to store the configuration `<some folder>`
- The minimal directory structure should look like this. Can be copied from _./config_<br>

  ```code
  <some-folder>
  L- config.json
  ```

- Edit the files to match your setup

## Run

### >> ARM64 target platform only <<

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=kuksa.val2iotea-adapter-<version> -v <some folder>:/home/node/app/docker/kuksa.val2iotea/config  kuksa.val2iotea-adapter-arm64:<version>`

### >> AMD64 target platform only <<

- `docker run --log-opt max-size=1m --log-opt max-file=5 --network="host" -d=true --restart=unless-stopped --name=kuksa.val2iotea-adapter-<version> -v <some folder>:/app/docker/kuksa.val2iotea/config kuksa.val2iotea-adapter-amd64:<version>`

### >> Linux only <<

- You have to prepend `sudo` to the docker call if you run docker as root (and you are not)
