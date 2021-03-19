<!---
  Copyright (c) 2021 Bosch.IO GmbH

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at https://mozilla.org/MPL/2.0/.

  SPDX-License-Identifier: MPL-2.0
-->

# GitHub action to check for signed-off commits in a given PR

## Introduction

This action checks whether all commits in a given PR are signed off

## Build

- Install _@vercel/ncc_ using `npm i -g @vercel/ncc` globally
- Build the action using `ncc build index.js --license licenses.txt`
