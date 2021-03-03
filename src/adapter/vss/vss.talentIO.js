/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const Ajv = require('ajv');

const {
    TalentIO
} = require('../../core/util/talentIO');

class VssInputValue {}

VssInputValue.validator = new Ajv().compile({
    type: 'object',
    required: [ 'sub' ],
    properties: {
        sub: {
            type: 'string',
            minLength: 1
        }
    }
});

VssInputValue.getSubscription = function (value) {
    return TalentIO.__ensureModel(value, VssInputValue.validator).get('sub');
};

class VssOutputValue {}

VssOutputValue.create = function (talent, subscription, value) {
    return {
        value,
        $vpath: 'value',
        sub: subscription,
        source: talent.id
    };
};

module.exports = {
    VssInputValue,
    VssOutputValue
};
