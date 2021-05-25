<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Change Log

## [Unreleased]

- 0.5.0, js-sdk-0.5.0, py-sdk-0.5.0
  - js-sdk, py-sdk: Add support for Protocol Gateway feature

- 0.4.1, js-sdk-0.4.1, py-sdk-0.4.1, vscode-sdk-0.9.4 [2021-05-05]
  - Function call timeouts are now correctly transported to succeeding function calls (other function calls, or recursive calls)
  - Fix range-selection in jsonQuery module to mimic the behavior of Python range selectors
  - py-sdk: Handle high event loads (e.g. recursive function calls) to prevent timeouts by configuring the ThreadPoolExecutor
  - js-sdk: Fix wrong export of platform event topics
  - js-sdk: Strip MQTT namespace from topics within the MQTT protocol adapter
  - js-sdk: Include sdk builds in package json script `yarn build.sdk`
  - vscode-sdk: Make refresh time interval for type features configurable

- 0.4.0, js-sdk-0.4.0, py-sdk-0.4.0, vscode-sdk-0.9.3 [2021-03-30]
  - Fix rule evaluation for Function Talents by adding excludeOn option for AND/OR rules.<br>
    Whenever a talent listened to function outputs (_callees()_ returns a non-empty array), the function output rule remained being evaluated to true once a single function response was successfully received. Talent output rules and (optional) trigger rules (returned by _getRules_ or _get\_rules_) were joined by an OR rule. Now any non-function event, belonging to a feature, which was defined in the trigger rules made the whole ruleset evaluate to true, no matter if the trigger rules were matched or not.
    - JSON API is now at 2.0.0
  - Homogenize usage of _getRules_, _get\_rules()_, _onEvent_ and _on\_event_ throughout Talents and Function Talents<br>
    Use the given functions no matter, if you are subclassing a Talent or a Function Talent
  - Remove dapr adapter example, since it's implementation is deprecated
  - Add bugfixes for Visual Studio Code Extension
    - Fix wrong evaluation of docker-compose build number
    - New troubleshooting section in main documentation
  - Remove deprecated dapr-adapter and dapr-example
