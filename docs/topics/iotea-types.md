<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# IoT Event Analytics - Internal data model

## Introduction

IoT Event Analytics organizes it's given `segment` and `types` and their corresponding feature definitions in the _types.json_ file.<br>

## Segments and Types

A Segment contains at least one defined type. It acts like a Super-Class, so all features, which are defined on the segment-level are inherited to all defined types. In The IoT Event Analytics context, a segment is an [eCl@ss](https://www.eclass.eu/index.html), which stands for a specific group of IoT devices e.g. a car. Segment features can be overridden in the type definition. The _idx_ field of a feature __must__ be unique within the whole segment.

### Example

`"280101"` is the eCl@ss for car. A possible type would be a specific model of a car e.g. `<Brand>-<Model>-<ReleaseYear>`. A segment feature would be "speed", since all cars can move at a certain speed. The drive mode selection will apply only to a subset of the given eCl@ss. So it has to be moved within the specific type.

```json
{
    "280101": {
        "features": {
            "speed": {
                // Segment-wide feature definition i.e. All types contained in 280101.types inherit the given feature
            }
        },
        "types": {
            "brand-model-2021": {
                "speed": {
                    // Extend inherited feature definition from segment with type specific definitions
                },
                "features": {
                    "drivingMode": {
                      // Type feature definition i.e. Only this type (here: brand-model-2021) has the given feature
                    }
                }
            }
        }
    }
}
```

## Instance

Event if an instance does not belong to the type definition itself, an instance resembles a real device at runtime level - it always instantiates a given type and is the runtime storage for features.

## Feature

A feature is the definition of a datapoint, which is always attached to a given _type_ or _segment_ (see above).

Example:

```json
{
    "speed": {
        "idx": 0,
        "history": 10,
        "ttl": 60000,
        "description": "Current vehicle speed",
        "unit": "KMH",
        "encoding": {
            "type": "number",
            "min": -250,
            "max": 250,
            "encoder": "minmax"
        }
    }
}

```

- Index (__*idx*__): Defines the index, at which the actual value of a feature is stored.
- History count (__*history*__=1): Defines how many historic values should be stored of this specific feature
- Time to live (__*ttl*__=30000): Defines how many Milliseconds a value should be defined. If a value is not updated for 30000 Milliseconds, it vanishes from the given instance.
- Description (__*description*__): Describes the given feature in a human readable form
- Unit of measurement (__*unit*__): The unit, in which the feature is measured<br>
  It can be one of the following

  ```json
  EITHER            // Use a predefined unit of measurement

  {
      "unit": "°C"  // °C is a key taken from the uom.json file
  }

  OR                // Define your own unit of measurement

  {
      "unit": {
          "fac": 1,
          "unit": "cust",
          "desc": "My custom unit"
      }
  }

  OR                // Extend an existing unit of measurement by a related one

  {
      "unit": {
          "fac": 0.001,  // The given factor scales the new unit to the base unit
          "unit": "mA",
          "desc": "Milliampere",
          "ref": "A"
      }
  }
  ```

- Encoding metadata (__*encoding*__)
  - Expected value type (__*type*__) can be one of `[string, boolean, number, array, object, any]`
  - Value encoder (__*encoder*__) can be one of
    - `minmax`: Encodes numeric values by mapping a defined input value range `[min..max]` to an output range between `[0..1]`. Values exceeding the range will be rejected with an out of bounds exception<br>
      Additional required properties (same level as _encoder_)
      - Minimum value (__*min*__)
      - Maximum value (__*max*__)
    - `category`: Encodes the index of a given set of values to an output range between `[0..1]`<br>
      Additional required properties (same level as _encoder_)
      - Possible values (__*enum*__) given as array
    - `null`: Value does not get encoded
    - `through`: Value is treated as "already encoded" and just copied over (Will be checked, if the given value is between `[0..1]`)
    - `delta`: Calculates the difference between the current and the previous value. Used to encode numeric values, which are e.g. continuously rising<br>
      Additional required properties (same level as _encoder_)
      - Minimum deviation (__*min*__=0)
      - Maximum deviation (__*max*__)
