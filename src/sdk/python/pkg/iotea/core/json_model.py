##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from .json_query import json_query_first

class JsonModel:
    def __init__(self, model):
        self.model = model

    def get(self, path, default_value=None):
        try:
            return json_query_first(self.model, path)['value']
        except Exception as ex:
            if default_value is not None:
                return default_value

            raise ex
