iotea-cli is a command line utility to simplify the interaction with the installation and configuration of the IoTEA platform.

# Installation

To install the latest release of iotea-cli:

`npm install iotea-cli`

# Pre-Requisites

- NodeJS v12 or higher
- NPM
- Docker
- TCP port 1883 (MQTT) is unused

# Usage

To get an overview of available commands:
`# iotea --help`

```
IoT Event Analytics CLI 0.0.1
Copyright (c) 2021 Robert Bosch GmbH
Usage: iotea <command> [options]

Commands:
  iotea start           Start the IoT Event Analytics Platform
  iotea run-int-tests   Run containerized integration tests
  iotea run-unit-tests  Unit tests with coverage report
  iotea install-vsix    Download and install Visual Studio Code Extension
  iotea stop            Stop the platform
  iotea get-sdks        Downloads latest SDK releases
  iotea status          Check the status of a running platform
  iotea verify-project  Verify if current project is configured correctly

Options:
      --version   Show version number                                  [boolean]
  -e, --env       A preconfigured environment
           [choices: "default-amd64", "default-arm64", "integrationtests-amd64",
               "integrationtests-arm64", "localproxy-amd64", "localproxy-arm64",
              "localproxy-integrationtests-amd64"] [default: "localproxy-amd64"]
  -f, --env-file  An optional docker-compose .env file                  [string]
  -v, --verbose   Verbose logging level (-v for debug, -vv for trace). If set,
                  docker output will go to stdout                        [count]
      --insecure  Disable SSL-Certificate validation for corporate proxies
  -h, --help      Show help                                            [boolean]

Copyright Robert Bosch GmbH, 2021
```

## Starting and stopping the IoTEA Platform

Using the _start_ command will pull the latest released IoTEA container images from a public container registry.

- Start: `iotea start`
- Stop: `iotea stop`

The IoTEA platform will start an embedded Mosquitto server on default port 1883 and expose it to the host network.

## Installing IoTEA Visual Studio Code Extension

Pre-Requisuite:
- Visual Studio Code installed

To install the latest release of the Visual Studio Code extension of IoT Event Analytics, use the _iotea install-vsix_ command:

```
# iotea install-vsix

IoT Event Analytics CLI 0.0.1
Copyright (c) 2021 Robert Bosch GmbH
[2021-09-28 13:48:28.281 +0000] INFO: Installing Visual Studio Code extension...
[2021-09-28 13:48:28.712 +0000] INFO: Found installed Visual Studio Code version: 1.60.2
[2021-09-28 13:48:28.713 +0000] INFO: Using http proxy: "localhost" "3128" ""
[2021-09-28 13:48:28.761 +0000] INFO: Requesting releases from https://api.github.com/repos/GENIVI/iot-event-analytics/releases
[2021-09-28 13:48:29.380 +0000] INFO: Found IoTEA Release vscode-ext-0.9.9
[2021-09-28 13:48:29.382 +0000] INFO: Downloading from: https://github.com/GENIVI/iot-event-analytics/releases/download/vscode-ext-0.9.9/iotea-0.9.9.vsix and writing to iotea-0.9.9.vsix
[2021-09-28 13:48:30.808 +0000] INFO: Download finished, installing Visual Studio Code extension
[2021-09-28 13:48:33.085 +0000] INFO: Visual Studio Output: Installing extensions...Extension 'iotea-0.9.9.vsix' was successfully installed.
```

After the installation, restart Visual Studio Code or use _[F1] - Developer: Reload Window_ so that the changes take effect.

## Running Talent Tests

Within an IoTEA project, run the following commands to execute unit tests and containerized integration tests:

### Unit Tests

`iotea run-unit-tests`

### Containerized Integration Tests

`iotea run-int-tests`

The test report will be written to _junit.xml_ 

_Note:_ For containerized integration tests, an additional configuration _config.json_ needs to be provided with a list of test suites.

## Proxy Configuration

To ease the setup for corporate environments, iotea-cli contains pre-defined environments for corporate setup. A pre-requisite is a running personal HTTP proxy server (such as px) on localhost:3128. iotea-cli will auto-detect proxy setup if there are HTTP_PROXY and HTTPS_PROXY environment variables set on the host environment and forward the configuration to the Docker images.

## Exit Codes

For scripting purposes, the _iotea-cli_ is returning exit codes for successful respectively for unsuccessful execution of the commands.

# Building from sources

Check out from source code repository and run:

`yarn install`

This will download and install all dependencies.
Finally, the binary `iotea` is installed into the path, so that it can be used in new shells.

# Release

The _release_ build step will build, tag and publish the release to npmjs.org:

`yarn release`
