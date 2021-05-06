/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const {
    MqttClient,
    NamedMqttClient,
    MqttProtocolAdapter
} = require('./core/util/mqttClient');
const Logger = require('./core/util/logger');

const MetadataManager = require('./core/metadataManager');

const Talent = require('./core/talent');
const FunctionTalent = require('./core/talent.func');

const {
    TestSetTalent,
    TestRunnerTalent
} = require('./core/talent.test');


const {
    TalentInput,
    TalentIO,
    TalentOutput
} = require('./core/util/talentIO');

const {
    Mapper,
    Worker,
    Reducer
} = require('./core/talent.mr');

const {
    ALL_INSTANCE_IDS_FILTER,
    ALL_SEGMENTS,
    ALL_TYPES,
    ANY_FEATURE,
    AndRules,
    OrRules,
    ChangeConstraint,
    OpConstraint,
    Rule
} = require('./core/rules');

const {
    DEFAULT_SEGMENT,
    DEFAULT_TYPE,
    DEFAULT_INSTANCE,
    ENCODING_ENCODER_CATEGORY,
    ENCODING_ENCODER_DELTA,
    ENCODING_ENCODER_MINMAX,
    ENCODING_ENCODER_THROUGH,
    ENCODING_TYPE_ANY,
    ENCODING_TYPE_BOOLEAN,
    ENCODING_TYPE_NUMBER,
    ENCODING_TYPE_OBJECT,
    ENCODING_TYPE_STRING,
    INGESTION_TOPIC,
    VALUE_TYPE_ENCODED,
    VALUE_TYPE_RAW,
    PLATFORM_EVENTS_TOPIC,
    PLATFORM_EVENT_TYPE_SET_CONFIG,
    PLATFORM_EVENT_TYPE_UNSET_CONFIG,
    GET_TEST_INFO_METHOD_NAME,
    PREPARE_TEST_SET_METHOD_NAME,
    RUN_TEST_METHOD_NAME,
    TEST_ERROR
} = require('./core/constants');

const {
    NelsonAlterConstraint,
    NelsonBiasConstraint,
    NelsonHighDevConstraint,
    NelsonLowDevConstraint,
    NelsonOut1SeConstraint,
    NelsonOut2SeConstraint,
    NelsonOut3SeConstraint,
    NelsonTrendConstraint
} = require('./core/rules.nelson');

const {
    TimeseriesPatternConstraint
} = require('./core/rules.tseries');

const {
    Wildcard
} = require('./core/util/arrayPatternMatcher');

const JsonModel = require('./core/util/jsonModel');
const jsonQuery = require('./core/util/jsonQuery');

const {
    VssAdapter ,
    VssPathTranslator
} = require('./adapter/vss/vss.adapter');

const ProtocolGateway = require('./core/protocolGateway');

const VssWebsocket = require('./adapter/vss/vss.websocket');

const {
    VssInputValue,
    VssOutputValue
} = require('./adapter/vss/vss.talentIO');

module.exports = {
    adapter: {
        VssAdapter,
        VssPathTranslator,
        VssWebsocket,
        VssInputValue,
        VssOutputValue
    },
    core: {
        MetadataManagerFactory: {
            createNewInstance: (connectionString) => {
                const metadataManager = new MetadataManager(connectionString);
                return metadataManager.start().then(() => metadataManager);
            }
        }
    },
    util: {
        MqttClient,
        NamedMqttClient,
        MqttProtocolAdapter,
        Logger,
        JsonModel,
        jsonQuery
    },
    constants: {
        ALL_INSTANCE_IDS_FILTER,
        ALL_SEGMENTS,
        ALL_TYPES,
        ANY_FEATURE,
        DEFAULT_SEGMENT,
        DEFAULT_TYPE,
        DEFAULT_INSTANCE,
        ENCODING_ENCODER_CATEGORY,
        ENCODING_ENCODER_DELTA,
        ENCODING_ENCODER_MINMAX,
        ENCODING_ENCODER_THROUGH,
        ENCODING_TYPE_ANY,
        ENCODING_TYPE_BOOLEAN,
        ENCODING_TYPE_NUMBER,
        ENCODING_TYPE_OBJECT,
        ENCODING_TYPE_STRING,
        INGESTION_TOPIC,
        VALUE_TYPE_ENCODED,
        VALUE_TYPE_RAW,
        PLATFORM_EVENTS_TOPIC,
        PLATFORM_EVENT_TYPE_SET_CONFIG,
        PLATFORM_EVENT_TYPE_UNSET_CONFIG,
        GET_TEST_INFO_METHOD_NAME,
        PREPARE_TEST_SET_METHOD_NAME,
        RUN_TEST_METHOD_NAME,
        TEST_ERROR
    },
    Talent,
    FunctionTalent,
    TestSetTalent,
    TestRunnerTalent,
    Mapper,
    Worker,
    Reducer,
    TalentInput,
    TalentOutput,
    TalentIO,
    AndRules,
    OrRules,
    Rule,
    ChangeConstraint,
    ProtocolGateway,
    OpConstraint,
    NelsonAlterConstraint,
    NelsonBiasConstraint,
    NelsonHighDevConstraint,
    NelsonLowDevConstraint,
    NelsonOut1SeConstraint,
    NelsonOut2SeConstraint,
    NelsonOut3SeConstraint,
    NelsonTrendConstraint,
    TimeseriesPatternConstraint,
    Wildcard
};