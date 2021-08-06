#!/bin/bash

##############################################################################
# Copyright (c) 2021 Robert Bosch GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

cd ../../docker-compose

ENV_FILE=.env-test

if [ $# -eq 0 ]
  then
    echo "No .env file provided. Will use .env-test."
else
  ENV_FILE=$1
fi

if [ ! -f "$ENV_FILE" ]; then
    echo "Can't find: $ENV_FILE"
    echo "Usage $0 {*.env file}"
    exit 1
fi


#Build test suite and runner
docker-compose -f docker-compose.integration_tests_js.yml -f docker-compose.integration_tests_py.yml -f docker-compose.integration_tests_runner.yml --env-file $ENV_FILE build --parallel

#Start platform & ALL SDK test-suites
docker-compose -f docker-compose.integration_tests_js.yml -f docker-compose.integration_tests_py.yml --env-file $ENV_FILE up -d

#Start integration-test runner
docker-compose -f docker-compose.integration_tests_runner.yml --env-file $ENV_FILE up
