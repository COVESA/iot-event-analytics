##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import time
import math
from datetime import datetime

def time_ms():
    dt_now = datetime.now()
    # Get rid of fractions of a second if set and add the current milliseconds
    return int(math.floor(time.time()) * 1000 + math.floor(dt_now.microsecond / 1000))