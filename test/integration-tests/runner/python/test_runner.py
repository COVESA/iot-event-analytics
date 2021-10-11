##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################
import asyncio
import logging

import json

from iotea.core.talent_test import TestRunnerTalent
from iotea.core.util.logger import Logger

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)


class TestRunner(TestRunnerTalent):
    def __init__(self, config):
        super(TestRunner, self).__init__('testRunner', config)


def read_config(abs_path):
    with open(abs_path, mode='r', encoding='utf-8') as config_file:
        return json.loads(config_file.read())


async def main():
    config = read_config('../../config/tests/python/runner/config.json')

    log_level = logging.INFO
    try:
        log_level = Logger.resolve_log_level(config.get('loglevel', log_level))
    finally:
        logging.getLogger().setLevel(log_level)

    test_runner = TestRunner(config)
    await test_runner.start()

    for task in asyncio.all_tasks():
        task.cancel()
        try:
            await task
        except asyncio.exceptions.CancelledError:
            # do nothing - clean up at exit
            pass
    asyncio.get_event_loop().stop()


if __name__ == '__main__':
    LOOP = asyncio.get_event_loop()
    LOOP.run_until_complete(main())
    LOOP.close()
