/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const JSON_API_VERSION = require('../../package.json')['jsonApiVersion'];

const TALENTS_DISCOVERY_TOPIC = 'configManager/talents/discover';
const TALENTS_DISCOVERY_RETURN_TOPIC = 'configManager/talent/discover';
const UPDATE_TALENT_CONFIG_TOPIC = 'talentConfigManager/update';
const UPDATE_TYPES_TOPIC = 'metadataManager/update';
const UPDATE_FEATURE_TOPIC = 'instanceManager/update';
const INGESTION_TOPIC = 'ingestion/events';
const ENCODING_TOPIC = 'encoding/events';
const ROUTING_TOPIC = 'routing/events';
const PLATFORM_EVENTS_TOPIC = 'platform/$events';

const PLATFORM_EVENT_TYPE_SET_CONFIG = 'platform.talent.config.set';
const PLATFORM_EVENT_TYPE_UNSET_CONFIG = 'platform.talent.config.unset';

const ENCODING_TYPE_NUMBER = 'number';
const ENCODING_TYPE_OBJECT = 'object';
const ENCODING_TYPE_STRING = 'string';
const ENCODING_TYPE_BOOLEAN = 'boolean';
const ENCODING_TYPE_ANY = 'any';

const ENCODING_ENCODER_THROUGH = 'through';
const ENCODING_ENCODER_MINMAX = 'minmax';
const ENCODING_ENCODER_DELTA = 'delta';
const ENCODING_ENCODER_CATEGORY = 'category';

const MSG_TYPE_EVENT = 1;
const MSG_TYPE_DISCOVERY = 2;
const MSG_TYPE_ERROR = 4;

const DEFAULT_SEGMENT = '000000';
const DEFAULT_TYPE = 'default';
const DEFAULT_INSTANCE = 'default';

const TALENT_DISCOVERY_INTERVAL_MS = 60000;
const TALENT_DISCOVERY_TIMEOUT_MS = 5000;
const TALENT_DISCOVERY_OPTIONS = {
    SKIP_CYCLE_CHECK: 'scc'
}
const DEFAULT_FEATURE_TTL_MS = 30000;

const VALUE_TYPE_RAW = 0;
const VALUE_TYPE_ENCODED = 1;

// If nothing is defined for a type, there will be always one entry in the history
const DEFAULT_HISTORY_LENGTH = 1;

const ERROR_NON_PREFIXED_FEATURE = 4000;
const ERROR_FEATURE_DEPENDENCY_LOOP = 4001;
const ERROR_INVALID_DISCOVERY_INFO = 4002;
const ERROR_RESOLVING_TYPES = 4003;

const CHANNEL_STEP_VALIDATE = 'validate';
const CHANNEL_STEP_TRANSFORM = 'transform';

const GET_TEST_INFO_METHOD_NAME = 'getTestSetInfo';
const PREPARE_TEST_SET_METHOD_NAME = 'prepare';
const RUN_TEST_METHOD_NAME = 'runTest';
const TEST_ERROR = 'TEST_ERROR';

module.exports = {
    TALENTS_DISCOVERY_TOPIC,
    TALENTS_DISCOVERY_RETURN_TOPIC,
    UPDATE_TALENT_CONFIG_TOPIC,
    UPDATE_TYPES_TOPIC,
    UPDATE_FEATURE_TOPIC,
    INGESTION_TOPIC,
    ENCODING_TOPIC,
    ROUTING_TOPIC,
    PLATFORM_EVENTS_TOPIC,
    MSG_TYPE_EVENT,
    MSG_TYPE_DISCOVERY,
    MSG_TYPE_ERROR,
    DEFAULT_SEGMENT,
    DEFAULT_TYPE,
    DEFAULT_INSTANCE,
    TALENT_DISCOVERY_INTERVAL_MS,
    TALENT_DISCOVERY_TIMEOUT_MS,
    TALENT_DISCOVERY_OPTIONS,
    DEFAULT_FEATURE_TTL_MS,
    DEFAULT_HISTORY_LENGTH,
    VALUE_TYPE_RAW,
    VALUE_TYPE_ENCODED,
    ERROR_FEATURE_DEPENDENCY_LOOP,
    ERROR_NON_PREFIXED_FEATURE,
    ERROR_INVALID_DISCOVERY_INFO,
    ERROR_RESOLVING_TYPES,
    CHANNEL_STEP_TRANSFORM,
    CHANNEL_STEP_VALIDATE,
    ENCODING_TYPE_NUMBER,
    ENCODING_TYPE_OBJECT,
    ENCODING_TYPE_STRING,
    ENCODING_TYPE_ANY,
    ENCODING_TYPE_BOOLEAN,
    ENCODING_ENCODER_CATEGORY,
    ENCODING_ENCODER_DELTA,
    ENCODING_ENCODER_MINMAX,
    ENCODING_ENCODER_THROUGH,
    PLATFORM_EVENT_TYPE_SET_CONFIG,
    PLATFORM_EVENT_TYPE_UNSET_CONFIG,
    JSON_API_VERSION,
    GET_TEST_INFO_METHOD_NAME,
    PREPARE_TEST_SET_METHOD_NAME,
    RUN_TEST_METHOD_NAME,
    TEST_ERROR
};