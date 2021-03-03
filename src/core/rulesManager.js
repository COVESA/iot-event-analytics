/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const uuid = require('uuid').v4;

const PlatformEvents = require('./platformEvents');
const Logger = require('./util/logger');
const { NamedMqttBroker } = require('./util/mqttBroker');

const {
    UPDATE_RULES_TOPIC,
    PLATFORM_EVENT_TYPE_SET_RULES,
    PLATFORM_EVENT_TYPE_UNSET_RULES
} = require('./constants');

const RulesLoader = require('./rules.loader');

module.exports = class RulesManager {
    constructor(connectionString) {
        this.uid = uuid();
        this.logger = new Logger(`RulesManager.${this.uid}`);
        this.broker = new NamedMqttBroker(this.logger.name, connectionString);
        this.ruleSets = {};
    }

    start() {
        return this.broker.subscribeJson(UPDATE_RULES_TOPIC, this.__onRulesUpdate.bind(this))
            .then(() => {
                this.logger.info(`RulesManager ${this.uid} started successfully`);
            });
    }

    getRuleSet(id) {
        if (!Object.prototype.hasOwnProperty.call(this.ruleSets, id)) {
            throw new Error(`Rules for given id ${id} not found`);
        }

        return this.ruleSets[id];
    }

    clearRules() {
        return this.broker.publishJson(UPDATE_RULES_TOPIC, {
            clear: true,
            source: this.uid
        });
    }

    unsetRules(id) {
        return this.broker.publishJson(UPDATE_RULES_TOPIC, {
            id,
            clear: true,
            source: this.uid
        });
    }

    setRules(id, rules, remote = false) {
        return this.broker.publishJson(UPDATE_RULES_TOPIC, {
            id,
            remote,
            rules,
            source: this.uid,
        });
    }

    getRuleIds() {
        return Object.keys(this.ruleSets);
    }

    async __onRulesUpdate(data) {
        if (data.clear === true) {
            if (data.id === undefined) {
                this.ruleSets = {};
                return;
            }

            if (Object.prototype.hasOwnProperty.call(this.ruleSets, data.id)) {
                if (data.source === this.uid) {
                    PlatformEvents.fire(PLATFORM_EVENT_TYPE_UNSET_RULES, {
                        talent: data.id,
                        rules: this.ruleSets[data.id].rules.save()
                    });
                }

                delete this.ruleSets[data.id];
            }

            return;
        }

        if (data.source === this.uid) {
            PlatformEvents.fire(PLATFORM_EVENT_TYPE_SET_RULES, {
                talent: data.id,
                rules: data.rules
            });
        }

        this.ruleSets[data.id] = {
            remote: data.remote || false,
            rules: RulesLoader.load(data.rules)
        };
    }
};