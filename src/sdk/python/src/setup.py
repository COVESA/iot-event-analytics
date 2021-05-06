##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from setuptools import setup

with open('README.md', 'r') as fh:
    LONG_DESCRIPTION = fh.read()

setup(
    name="boschio-iotea",
    version="0.4.1",
    author="Bosch.IO GmbH",
    description="Core library for Talent development",
    long_description=LONG_DESCRIPTION,
    long_description_content_type="text/markdown",
    url="https://github.com/GENIVI/iot-event-analytics",
    packages=[
        "iotea.core",
        "iotea.core.util"
    ],
    install_requires=[
        "websockets==8.1",
        "hbmqtt==0.9.6"
    ],
    classifiers=[
        "Programming Language :: Python :: 3",
        "License :: MPL-2.0 License",
        "Operation System :: OS Independent"
    ],
    python_requires=">=3.6"
)
