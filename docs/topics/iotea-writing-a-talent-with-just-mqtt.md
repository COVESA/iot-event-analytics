<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Writing a talent with only an MQTT client available

## A few things to clarifiy

- `<platformId>`: The unique platform identifier
- `<talentId>`: The unique talent identifier
- `<feature>`: The name of a talent feature
- `<callChannelId>`: Random UUIDv4 identifier for establishing a "private" communication channel to this specific talent instance, which will be defined on talent "instantiation" to return function results.
- `<ioteaTopicNs>`: The topic namespace which is used by the platform. Use "iotea/"

## Understand the message flow

### Prerequisites

- Your MQTT Client should be connected the the broker instance, the platform is connected to
  - Subscribe to the following topics:<br>
    Make sure your Client implementation supports wildcard subscriptions (_+_)
    - `$share/<ioteaTopicNs>/configManager/talents/discover` (_Topic A_)
    - `$share/<ioteaTopicNs>/talent/<talentId>/events` (_Topic B_)
    - `talent/<ioteaTopicNs>/events/<talentId>.<callChannelId>/+` (_Topic C_)

    Replace `<talentId>` with your talent ID (Just think of an ID) <br>
    Replace `<callChannelId>` with a uuid v4 random string for each talent instance <br>
    Replace `<ioteaTopicNs>`with the namespace used by the platform, e.g. "iotea"<br>

### Flow

- At the beginning, the platform does not know anything about any talent, so it runs a discovery.<br>
  Thus, you will cyclically receive discovery messages on _Topic A_ with `msgType=2`
- Your response will have to be sent to the given _returnTopic_ in the message
  - If any error occurs, you will be notified on _Topic B_ with the `msgType=4`
- From now on, you will receive events on _Topic B_ with the `msgType=1` as soon as your given ruleset is evaluated to _true_
  - If your talents also acts as a producer, you can return these values using the guide under _"Publishing API"_
- If you want to invoke functions from your talent, see the function invocation section under _"Publishing API"_
  - If you invoked a function, you will receive the result (can also be an error i.e. `msgType=4`) on _Topic C_

## Subscription API

- `$share/<talentId>/configManager/talents/discover` (_Topic A_)

  ```json
  // You receive this message

  {
    "msgType": 2,
    "version": "<JSON API SemVer>",
    "returnTopic": "<platformId>/configManager/talent/discover"
  }
  ```

  ```json
  // You answer to message.returnTopic (here: <platformId>/configManager/talent/discover)

  {
      "id": "<talentId>",
      "config": {
          "scc": [                                                                            // Skip cycle check (scc) for the following type features
              "default.<talentId>.fibonacci-out",
              "default.<talentId>.multiply-in",
              "default.<talentId>.fibonacci-in"
          ],
          "outputs": {                                                                        // Talent outputs
              "<talentId>.multiply-in": {                                                     // <feature> prefixed by <talentId>
                  "description": "Argument(s) for function multiply",                         // Metadata for the output
                  "encoding": {
                      "type": "object",                                                       // Object type validation input
                      "encoder": null                                                         // Encoding settings
                  },
                  "unit": "ONE"                                                               // Unit of measurement. see uom.json in platform configuration
              },
              "<talentId>.multiply-out": {
                  "description": "Result of function multiply",
                  "encoding": {
                      "type": "any",
                      "encoder": null
                  },
                  "unit": "ONE"
              },
              "<talentId>.fibonacci-in": {
                  "description": "Argument(s) for function fibonacci",
                  "encoding": {
                      "type": "object",
                      "encoder": null
                  },
                  "unit": "ONE"
              },
              "<talentId>.fibonacci-out": {
                  "description": "Result of function fibonacci",
                  "encoding": {
                      "type": "any",
                      "encoder": null
                  },
                  "unit": "ONE"
              }
          },
          "rules": {
              "type": "or",                                                                   // If one rule is evaluated to true, the whole ruleset is evaluated to true
              "excludeOn": null,
              "rules": [
                  {
                      "type": "or",                                                           // Rules for receiving function talent responses (defined in callees())
                      "excludeOn": null,
                      "rules": [
                          {
                              "path": "/$tsuffix",                                            // Inner path of the value to be checked
                              "feature": "<talentId>.fibonacci-out",                          // Name of the <feature>, which should be evaluated
                              "op": 0,                                                        // 0 = Schema constraint
                              "value": {                                                      // JSON Schema, which should be used
                                  "type": "string",
                                  "pattern": "^/<talentId>\.[^\/]+/.*"
                              },
                              "valueType": 0,                                                 // Evaluate 0 = RAW, 1 = ENCODED value
                              "typeSelector": "default",                                      // Selects the <type>. Can be used in conjunction with <segment> and wildcards
                              "instanceIdFilter": ".*",                                       // Evaluates only for <instance> for which the regex matches
                              "limitFeatureSelection": true                                   // Only applicable, if <feature> is a wildcard --> set to false to retrieve each and every feature in the $features section, otherwise only the $feature belonging to the event is contained
                          }
                      ]
                  },
                  {
                      "type": "or",                                                           // Rules for receiving function triggers (defined by registerFunction())
                      "excludeOn": [                                                          // Exclude all function talent responses
                          "<talent-id>.fibonacci-out"
                      ],
                      "rules": [
                          {
                              "path": "",
                              "feature": "<talentId>.multiply-in",
                              "op": 0,
                              "value": {
                                  "type": "object",
                                  "required": [
                                      "func",
                                      "args",
                                      "chnl",
                                      "call",
                                      "timeoutAtMs"
                                  ],
                                  "properties": {
                                      "func": {
                                          "type": "string",
                                          "const": "multiply"
                                      },
                                      "args": {
                                          "type": "array"
                                      },
                                      "chnl": {
                                          "type": "string"
                                      },
                                      "call": {
                                          "type": "string"
                                      },
                                      "timeoutAtMs": {
                                          "type": "integer"
                                      }
                                  },
                                  "additionalProperties": false
                              },
                              "valueType": 0,
                              "typeSelector": "default",
                              "instanceIdFilter": ".*",
                              "limitFeatureSelection": true
                          },
                          {
                              "path": "",
                              "feature": "<talentId>.fibonacci-in",
                              "op": 0,
                              "value": {
                                  "type": "object",
                                  "required": [
                                      "func",
                                      "args",
                                      "chnl",
                                      "call",
                                      "timeoutAtMs"
                                  ],
                                  "properties": {
                                      "func": {
                                          "type": "string",
                                          "const": "fibonacci"
                                      },
                                      "args": {
                                          "type": "array"
                                      },
                                      "chnl": {
                                          "type": "string"
                                      },
                                      "call": {
                                          "type": "string"
                                      },
                                      "timeoutAtMs": {
                                          "type": "integer"
                                      }
                                  },
                                  "additionalProperties": false
                              },
                              "valueType": 0,
                              "typeSelector": "default",
                              "instanceIdFilter": ".*",
                              "limitFeatureSelection": true
                          },
                          {
                              "type": "or",                                                    // Rules for custom function triggers (defined by getRules())
                              "excludeOn": [
                                  "<talentId>.multiply-in",
                                  "<talentId>.fibonacci-in"
                              ],
                              "rules": [
                                // Some additional trigger rules for your function talent (optional)
                              ]
                          }
                      ]
                  }
              ]
          }
      }
  }
  ```

- `$share/<talentId>/talent/<talentId>/events` (_Topic B_)

  ```json
  // You receive these messages

  {
      "msgType": 1,
      "subject": "",                                                                  // The owner of the <instance> of a given <type>
      "segment": "100000",                                                            // The <segment>, to which the given <type> belongs
      "type": "Vehicle",                                                              // The <type> of the device, the <feature> belongs to
      "feature": "inner-temperature",                                                 // Which <feature> of the given <type> changed
      "instance": "4711",                                                             // The <instance> of the given <type>
      "value": 20,                                                                    // The current value of the <feature>
      "whenMs": 1605026236000,                                                        // When was the <feature> changed as Unix timestamp in ms
      "$features": {                                                                  // Contains all features of "interest" i.e. given by the Ruleset
        "Vehicle": {
          "inner-temperature": {
            "$metadata": {
              "description": "The current in-car temperature",
              "encoding": {
                "type": "number",
                "encoder": "minmax",
                "min": -20,
                "max": 80
              },
              "history": 10,                                                                // How many history entries should be kept in the ringbuffer (Value in range of [0..50])
              "idx": 0                                                                      // The index in the row-vector of the given <type>
            },
            "4711": {
              "$feature": {
                "enc": 0.4,
                "raw": 20,
                "whenMs": 1605026236000,
                "history": [
                  <$feature>,                                                         // The previous values, if present
                  ...                                                                 // In this case, max. 10 values are kept
                ],
                "stat": {                                                             // Statistical information only available for encoded values
                  "cnt": 1,                                                           // How many values contributed to the running statistical values
                  "var": 0,                                                           // variance
                  "mean": 0.4,                                                        // arithmetic mean
                  "sdev": 0                                                           // standard deviation
                }
              },

            },
            ...
          },
          ...
        },
        ...
      },
      "returnTopic": "<returnTopic>"                                                  // If you want to return some data, publish it to this topic (see further down)
  }

  OR

  {
      "msgType": 4,
      "code": 4000                                                                    // 4000 = Non prefixed output feature found
                                                                                      // 4001 = Feature dependency loop found
                                                                                      // 4002 = Invalid discovery info
                                                                                      // 4003 = Error resolving given segment in the talent ruleset
  }
  ```

- `talent/<talentId>/events/<talentId>.<callChannelId>/<deferredCallId>` (_Topic C_)<br>
  Receive response messages from deferred function calls. The function invocation message is described in _"Publishing API"_ below
  - `deferredCallId` will be a random string, which will be generated when a function call is invoked.<br>
  You would subscribe to `talent/<talentId>/events/<talentId>.<callChannelId>/+`

  ```json
  // You receive the function result as event

  {
      "msgType": 1,                                                                   // 1 = success, 4 = error
      "subject": "",
      "segment": "default",
      "type": "default",
      "feature": "<talentId>.fibonacci-out",
      "instance": "default",
      "value": {
          "$tsuffix": "<talentId>.<callChannelId>/<deferredCallId>",                             // Routing metadata
          "$vpath": "value",                                                          // The path of the actual value payload (here: "value", can also be "error" in case of msgType = 4)
          "value": "42"
      },
      "whenMs": 1605026236000,
      "$features": {
        "default": {
          "<talentId>.fibonacci-out": {
            "$metadata": {
              "description": "Result of function multiply",
              "encoding": {
                  "type": "any",
                  "encoder": null
              },
              "unit": "ONE"
            },
            "default": {
              "$feature": {
                "enc": null,
                "raw": {
                    "$tsuffix": "<talentId>.<callChannelId>/<deferredCallId>",
                    "$vpath": "value",
                    "value": "42"
                },
                "whenMs": 1605026236000,
                "history": [
                  ...
                ]
              },
            },
          }
        }
      },
      "returnTopic": "<returnTopic>"
  }
  ```

## Publishing API

- Publish a message as a _reaction_ to a received event (always use the `returnTopic` of the received event)

  ```json
  // Talent acts as a producer for feature1 (optional)

  {
      "subject": <subject>,
      "feature": "<talentId>.feature1",
      "value": 123,
      "type": "default",                                                              // Automatically set to default
      "instance": "default",                                                          // Automatically set to default
      "whenMs": 1605026236000
  }

  If you need to return more than one value at once, you can just publish an Array-of-Json [{},{}, ...]
  ```

  ```json
  // Call a function from your talent (You need to be triggered by an event. Here: ev)

  {
      "subject": ev.subject,
      "type": "default",
      "feature": "<talentId>.fibonacci-in",
      "value": {
          "func": "fibonacci",
          "args": [ 8 ],                                                              // The function arguments as array !
          "call": "<deferredCallId>",                                                 // The random id of your call to transport the response
          "chnl": "<talentId>.<callChannelId>",
          "whenMs": 1605026236000,
          "timeoutAtMs": 1605026286000                                                // The point in time at which the caller times out
      }
  }

  IMPORTANT: Make sure you add <talentId>.fibonacci-in to your template ruleset (The template, which calls the function) and check for ISSET
  ```

- Publish an event to the ingestion topic of the platform `<ioteaTopicNs>/ingestion/events`

  ```json
  // Send an event directly to the platform

  {
      "subject": <subject>,
      "instance": <instanceId>,
      "feature": <featureName>,
      "type": <type>,                                                                 // If not "default" i.e. feature defined by some talent
      "value": <anyvalue>,
      "whenMs": 1605026236000
  }
  ```
