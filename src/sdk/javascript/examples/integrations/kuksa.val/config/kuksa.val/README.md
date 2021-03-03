<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Prerequisites

The contents of this folder has to look like

```code
certs
|- jwt.key.pub
|- Server.key
L- Server.pem
vss.json
```

You can find out, where to get these files [here](../../../../../../../../docker/vss2iotea/README.md)

Modify the _vss.json_ file as follows:

```json
{
  "Vehicle": {
    "uuid": "1c72453e738511e9b29ad46a6a4b77e9",
    "type": "branch",
    "children": {
      "UserId": {                                           // Add this UserId field
        "description": "UserId",
        "datatype": "string",
        "value": "ishouldbethesubject",
        "uuid": "ed39ffe684775330fdfd08f910e39b00",
        "type": "attribute"
      },
    }
  }
}

{
  "VIN": {
    "description": "17-character Vehicle Identification Number (VIN) as defined by ISO 3779",
    "datatype": "string",
    "type": "attribute",
    "uuid": "e32e165999585625a59247186b7007b4",
    "value": "ishouldbeavin"                                // Add a default value for the VIN
  }
}
```
