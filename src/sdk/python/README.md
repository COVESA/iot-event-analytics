<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Package

## Build

- Create an empty _./lib_ folder folder if not exists
- Go into the _./pkg_ folder

### Build Source distribution package

`python setup.py egg_info --egg-base ../lib sdist --dist-dir=../lib clean --all`

### Build Binary distribution

`python setup.py egg_info --egg-base ../lib bdist_wheel --dist-dir=../lib clean --all`

## Install

- Goto directory ./lib and pick a version you would like to install<br>
- `python -m pip install --user boschio_iotea-<version>-py3-none-any.whl --force`
