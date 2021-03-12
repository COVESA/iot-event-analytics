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
const ProtocolGateway = require('./protocolGateway');

const {
    UPDATE_TALENT_CONFIG_TOPIC,
    PLATFORM_EVENT_TYPE_SET_CONFIG,
    PLATFORM_EVENT_TYPE_UNSET_CONFIG
} = require('./constants');

const RulesLoader = require('./rules.loader');

module.exports = class TalentConfigManager {
    constructor(protocolGatewayConfig) {
        this.uid = uuid();
        this.logger = new Logger(`TalentConfigManager.${this.uid}`);
        this.pg = new ProtocolGateway(protocolGatewayConfig, this.logger.name, true);
        this.configurations = {};
    }

    start() {
        return this.pg.subscribeJson(UPDATE_TALENT_CONFIG_TOPIC, this.__onRulesUpdate.bind(this))
            .then(() => {
                this.logger.info(`TalentConfigManager ${this.uid} started successfully`);
            });
    }

    getConfig(id) {
        if (!Object.prototype.hasOwnProperty.call(this.configurations, id)) {
            throw new Error(`Configuration for given id ${id} not found`);
        }

        return this.configurations[id];
    }

    clearAll() {
        return this.pg.publishJson(UPDATE_TALENT_CONFIG_TOPIC, {
            clear: true,
            source: this.uid
        });
    }

    removeConfig(id) {
        return this.pg.publishJson(UPDATE_TALENT_CONFIG_TOPIC, {
            id,
            clear: true,
            source: this.uid
        });
    }

    setConfig(id, config) {
        return this.pg.publishJson(UPDATE_TALENT_CONFIG_TOPIC, {
            id,
            config,
            source: this.uid,
        });
    }

    getTalentIds() {
        return Object.keys(this.configurations);
    }

    async __onRulesUpdate(data) {
        if (data.clear === true) {
            if (data.id === undefined) {
                for (let id in this.configurations) {
                    await this.__fireUnsetConfigFor(id);
                }

                this.configurations = {};

                return;
            }

            if (Object.prototype.hasOwnProperty.call(this.configurations, data.id)) {
                if (data.source === this.uid) {
                    await this.__fireUnsetConfigFor(data.id);
                }

                delete this.configurations[data.id];
            }

            return;
        }

        this.configurations[data.id] = data.config;
        // Load the given rules
        this.configurations[data.id].rules = RulesLoader.load(this.configurations[data.id].rules);

        if (data.source === this.uid) {
            await this.__fireSetConfigFor(data.id, this.configurations[data.id]);
        }
    }

    __fireSetConfigFor(id) {
        return PlatformEvents.fire(PLATFORM_EVENT_TYPE_SET_CONFIG, this.__createConfigUpdatePlatformEventFor(id));
    }

    __fireUnsetConfigFor(id) {
        return PlatformEvents.fire(PLATFORM_EVENT_TYPE_UNSET_CONFIG, this.__createConfigUpdatePlatformEventFor(id));
    }

    __createConfigUpdatePlatformEventFor(id) {
        return {
            talent: id,
            config: {
                aid: this.configurations[id].aid,
                rules: this.configurations[id].rules.save()
            }
        };
    }
};