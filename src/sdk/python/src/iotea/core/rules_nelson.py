##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from .rules import Constraint

class TimeseriesConstraint(Constraint):
    # pylint: disable=useless-super-delegation
    def __init__(self, feature, op, value, type_selector, value_type, path, instance_id_filter, limit_feature_selection):
        super(TimeseriesConstraint, self).__init__(feature, op, value, type_selector, value_type, path, instance_id_filter, limit_feature_selection)

class _NelsonConstraint(TimeseriesConstraint):
    def __init__(self, feature, nelson_type, type_selector, instance_id_filter, limit_feature_selection):
        super(_NelsonConstraint, self).__init__(feature, Constraint.OPS['NELSON'], nelson_type, type_selector, Constraint.VALUE_TYPE['ENCODED'], Constraint.PATH_IDENTITY, instance_id_filter, limit_feature_selection)

_NelsonConstraint.NELSON_TYPE = {
    'OUT3_SE': 0,
    'OUT2_SE': 1,
    'OUT1_SE': 2,
    'BIAS': 3,
    'TREND': 4,
    'ALTER': 5,
    'LOW_DEV': 6,
    'HIGH_DEV': 7
}

class NelsonAlterConstraint(_NelsonConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonAlterConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['ALTER'], type_selector, instance_id_filter, limit_feature_selection)

class NelsonTrendConstraint(_NelsonConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonTrendConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['TREND'], type_selector, instance_id_filter, limit_feature_selection)

class NelsonBiasConstraint(_NelsonConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonBiasConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['BIAS'], type_selector, instance_id_filter, limit_feature_selection)

class _NelsonDevConstraint(_NelsonConstraint):
    def __init__(self, feature, nelson_type, type_selector, instance_id_filter, limit_feature_selection=True):
        super(_NelsonDevConstraint, self).__init__(feature, nelson_type, type_selector, instance_id_filter, limit_feature_selection)

class NelsonHighDevConstraint(_NelsonDevConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonHighDevConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['HIGH_DEV'], type_selector, instance_id_filter, limit_feature_selection)

class NelsonLowDevConstraint(_NelsonDevConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonLowDevConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['LOW_DEV'], type_selector, instance_id_filter, limit_feature_selection)

class _NelsonOutSeConstraint(_NelsonConstraint):
    def __init__(self, feature, nelson_type, type_selector, instance_id_filter, limit_feature_selection=True):
        super(_NelsonOutSeConstraint, self).__init__(feature, nelson_type, type_selector, instance_id_filter, limit_feature_selection)

class NelsonOut1SeConstraint(_NelsonOutSeConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonOut1SeConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['OUT1_SE'], type_selector, instance_id_filter, limit_feature_selection)

class NelsonOut2SeConstraint(_NelsonOutSeConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonOut2SeConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['OUT2_SE'], type_selector, instance_id_filter, limit_feature_selection)

class NelsonOut3SeConstraint(_NelsonOutSeConstraint):
    def __init__(self, feature, type_selector, instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER, limit_feature_selection=True):
        super(NelsonOut3SeConstraint, self).__init__(feature, _NelsonConstraint.NELSON_TYPE['OUT3_SE'], type_selector, instance_id_filter, limit_feature_selection)
