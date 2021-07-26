##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

from unittest import TestCase

import pytest

from src.iotea.core.rules import Constraint, Rule
from src.iotea.core.rules_nelson import NelsonAlterConstraint, NelsonTrendConstraint, NelsonBiasConstraint, \
    NelsonHighDevConstraint, NelsonLowDevConstraint, NelsonOut1SeConstraint, NelsonOut2SeConstraint, \
    NelsonOut3SeConstraint


@pytest.fixture
def test_case():
    return TestCase()


def test_alter_constraint(test_case):
    nelson_alter_constraint = NelsonAlterConstraint('TestFeature', 'anyType',
                                                    instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                    limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_alter_constraint.op)
    test_case.assertEqual(5, nelson_alter_constraint.value)  # Alter
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_alter_constraint.value_type)


def test_trend_constraint(test_case):
    nelson_trend_constraint = NelsonTrendConstraint('TestFeature', 'anyType',
                                                    instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                    limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_trend_constraint.op)
    test_case.assertEqual(4, nelson_trend_constraint.value)  # Trend
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_trend_constraint.value_type)


def test_bias_constraint(test_case):
    nelson_bias_constraint = NelsonBiasConstraint('TestFeature', 'anyType',
                                                  instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                  limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_bias_constraint.op)
    test_case.assertEqual(3, nelson_bias_constraint.value)  # Bias
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_bias_constraint.value_type)


def test_high_dev_constraint(test_case):
    nelson_high_dev_constraint = NelsonHighDevConstraint('TestFeature', 'anyType',
                                                         instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                         limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_high_dev_constraint.op)
    test_case.assertEqual(7, nelson_high_dev_constraint.value)  # High Dev
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_high_dev_constraint.value_type)


def test_low_dev_constraint(test_case):
    nelson_low_dev_constraint = NelsonLowDevConstraint('TestFeature', 'anyType',
                                                       instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                       limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_low_dev_constraint.op)
    test_case.assertEqual(6, nelson_low_dev_constraint.value)  # Low Dev
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_low_dev_constraint.value_type)

def test_out_1_se_constraint(test_case):
    nelson_out_1_se_constraint = NelsonOut1SeConstraint('TestFeature', 'anyType',
                                                       instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                       limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_out_1_se_constraint.op)
    test_case.assertEqual(2, nelson_out_1_se_constraint.value)  # Out 1 se
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_out_1_se_constraint.value_type)


def test_out_2_se_constraint(test_case):
    nelson_out_2_se_constraint = NelsonOut2SeConstraint('TestFeature', 'anyType',
                                                       instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                       limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_out_2_se_constraint.op)
    test_case.assertEqual(1, nelson_out_2_se_constraint.value)  # Out 2 se
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_out_2_se_constraint.value_type)


def test_out_3_se_constraint(test_case):
    nelson_out_3_se_constraint = NelsonOut3SeConstraint('TestFeature', 'anyType',
                                                       instance_id_filter=Constraint.ALL_INSTANCE_IDS_FILTER,
                                                       limit_feature_selection=True)
    test_case.assertEqual(Constraint.OPS['NELSON'], nelson_out_3_se_constraint.op)
    test_case.assertEqual(0, nelson_out_3_se_constraint.value)  # Out 3 se
    test_case.assertEqual(Constraint.VALUE_TYPE['ENCODED'], nelson_out_3_se_constraint.value_type)
