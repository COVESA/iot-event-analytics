<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics C++ SDK

To build the library, just create a build directory and configure/build with CMake.
This will automatically fetch the required dependencies (i.e. the Paho MQTT client).

1. `mkdir build`
2. `cd build`
3. `cmake ..`
4. `make`

## Examples

Example usage of the library can be found under /examples

All paths are related to the root of your build folder. That means:

1. You should execute your binaries from the root of your build folder
2. You should reference related configuration files relative to root of the build folder.

e.g.
```
cd <build-folder>
./examples/basic/basic examples/basic/pgconfig.json
```