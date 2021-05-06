##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from src.iotea.core.rules import ChangeConstraint, OpConstraint

def create_change_constraint(*args):
    oc = ChangeConstraint(*args)
    return oc

def create_op_constraint(*args):
    oc = OpConstraint(*args)
    oc.value['$id'] = None
    return oc