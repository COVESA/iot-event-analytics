<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Node-RED SDK

__Important:__ This SDK is under development, but a first node allows to have access on VAPP features as well as provide new one.

## Overview

Currently, it provides the following features:

1. A simple Node-RED Node 'VAPP Features'
2. An example flow using a 'VAPP Feature' (./imports/vapp_feature_node_example.json)
3. A Node-RED flow which defines a talent using only default nodes (./imports/talent_in_node-red.json)

## Install 'VAPP Features' Node-RED node

Install the VAPP Features node with (-g if it shall be globally installed)

```code
npm install ./nodes [-g]
```

After restarting Node-RED you should see "VAPP Features" in the palette on the left side

```code
node-red
```

## Example: Usage of 'VAPP Features' node

This flow shows an example how to use a 'VAPP Features' node in Node-RED.

To try it out you have to install 'VAPP Features' as described above and to import it into your Node-RED as a flow.

## Example: Talent in node-red

This flow shows how you can implement a talent in Node-RED using default nodes of Node-RED.

To try it out you have to import it into your Node-RED as a flow.

## Hints & Known Issues

- Currently, Feature _Body$Windshield$Front$Wiping$Status_ for type _Vehicle_ is used. Either define it in your platform features or change it to an existing one. If you are changing it to a different one, please be aware that that the type _Vehicle_ is hardcoded in the discovery sequence (talent_in_node-red.json).
- Currently, the IP address for discovery is hardcoded in vapp-feat.js (set to 127.0.0.1 aka localhost). If you have to change it: Change it in the local file and (re-)install the package _nodes_ as mentioned [above](## Install 'VAPP Features' Node-RED node)
