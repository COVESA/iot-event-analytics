##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

ONE_SECOND_MS = 1000

TALENTS_DISCOVERY_TOPIC = 'configManager/talents/discover'
TALENTS_DISCOVERY_RETURN_TOPIC = 'configManager/talent/discover'
UPDATE_TALENT_CONFIG_TOPIC = 'talentConfigManager/update'
UPDATE_TYPES_TOPIC = 'metadataManager/update'
UPDATE_FEATURE_TOPIC = 'instanceManager/update'
INGESTION_TOPIC = 'ingestion/events'
ENCODING_TOPIC = 'encoding/events'
ROUTING_TOPIC = 'routing/events'
PLATFORM_EVENTS_TOPIC = 'platform/$events'

PLATFORM_EVENT_TYPE_SET_CONFIG = 'platform.talent.config.set'
PLATFORM_EVENT_TYPE_UNSET_CONFIG = 'platform.talent.config.unset'

ENCODING_TYPE_NUMBER = 'number'
ENCODING_TYPE_OBJECT = 'object'
ENCODING_TYPE_STRING = 'string'
ENCODING_TYPE_BOOLEAN = 'boolean'
ENCODING_TYPE_ANY = 'any'

ENCODING_ENCODER_THROUGH = 'through'
ENCODING_ENCODER_MINMAX = 'minmax'
ENCODING_ENCODER_DELTA = 'delta'
ENCODING_ENCODER_CATEGORY = 'category'

MSG_TYPE_EVENT = 1
MSG_TYPE_DISCOVERY = 2
MSG_TYPE_ERROR = 4

DEFAULT_SEGMENT = '000000'
DEFAULT_TYPE = 'default'
DEFAULT_INSTANCE = 'default'

TALENT_DISCOVERY_INTERVAL_MS = 60 * ONE_SECOND_MS
TALENT_DISCOVERY_TIMEOUT_MS = 5 * ONE_SECOND_MS
TALENT_DISCOVERY_OPTIONS = {
    'SKIP_CYCLE_CHECK': 'scc'
}

MAX_TALENT_EVENT_WORKER_COUNT = 1024

DEFAULT_FEATURE_TTL_MS = 30 * ONE_SECOND_MS

VALUE_TYPE_RAW = 0
VALUE_TYPE_ENCODED = 1

ERROR_NON_PREFIXED_FEATURE = 4000
ERROR_FEATURE_DEPENDENCY_LOOP = 4001
ERROR_INVALID_DISCOVERY_INFO = 4002
ERROR_RESOLVING_TYPES = 4003

CHANNEL_STEP_VALIDATE = 'validate'
CHANNEL_STEP_TRANSFORM = 'transform'

GET_TEST_INFO_METHOD_NAME = 'getTestSetInfo'
PREPARE_TEST_SET_METHOD_NAME = 'prepare'
RUN_TEST_METHOD_NAME = 'runTest'
TEST_ERROR = 'TEST_ERROR'
