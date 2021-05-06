<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# Package

## Test

- Run `pytest` after installing all dependencies
  - After running the tests, you will find all reports in the _./reports_ directory. It'll contain coverage reports as HTML and XML in cobertura format.

## Build and install in one go

`python -m pip install ./src --user --force`

## Build

- Create an empty _./lib_ folder folder if not exists
- Go into the _./src_ folder

### Build Source distribution package

`python setup.py egg_info --egg-base ../lib sdist --dist-dir=../lib clean --all`

### Build Binary distribution

`python setup.py egg_info --egg-base ../lib bdist_wheel --dist-dir=../lib clean --all`

## Install

- Goto directory ./lib and pick a version you would like to install<br>
- `python -m pip install --user boschio_iotea-<version>-py3-none-any.whl --force`

## Test
