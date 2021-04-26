<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Convert the GENIVI VSS JSON data model into IoT Event Analytics Segment and Type representation

## Prerequisites

- Build GENIVI Reference JSON model from vspec as described [https://github.com/GENIVI/vehicle_signal_specification/tree/master](https://github.com/GENIVI/vehicle_signal_specification/tree/master) using the provided vss-tools
- Run `node vss2types.js -s <segment> -v <vss file built above> -u ./vss.uom.json -o types.json`
  - Specify option `-i <path to your existing types.json>` if you have already a _types.json_ document for your IoT Event Analytics Platform available
  - Specify option `-f true` if you would like to overwrite an existing segment in your _types.json_ document (e.g. if you want to update it)
- Run `node vss2types -h` to display help
