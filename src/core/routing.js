/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const { NamedMqttBroker } = require('./util/mqttBroker');

const { Rules } = require('./rules');
const Talent = require('./talent');

const RulesManager = require('./rulesManager');
const InstanceManager = require('./instanceManager');

const Logger = require('./util/logger');
const FeatureMap = require('./util/featureMap');

const {
    INGESTION_TOPIC,
    ROUTING_TOPIC
} = require('./constants');

module.exports = class Routing {
    constructor(connectionString, platformId = '') {
        this.logger = new Logger('Routing');
        this.platformId = platformId;
        this.broker = new NamedMqttBroker('Routing', connectionString);
        this.instanceManager = new InstanceManager(connectionString);
        this.rulesManager = new RulesManager(connectionString);
    }

    start() {
        return this.instanceManager.start()
            .then(() => this.rulesManager.start())
            .then(() => this.broker.subscribeJson(`$share/routing/${ROUTING_TOPIC}`, this.__onEvent.bind(this)))
            .then(() => {
                this.logger.info('Routing started successfully');
            });
    }

    async __onEvent(ev) {
        await this.__handleEvent(ev);
    }

    async __handleEvent(ev) {
        const evtctx = Logger.createEventContext(ev);

        try {
            for(const id of this.rulesManager.getRuleIds()) {
                this.logger.debug(`Evaluating rules for talent ${id}...`, evtctx);

                const ruleset = this.rulesManager.getRuleSet(id);

                const typeFeatures = await ruleset.rules.getUniqueTypeFeatures();
                const evTypeFeature = Rules.getTypeFeature(ev.type, ev.feature, ev.segment);

                // Is the current event part of the rules feature types? If not, the event does not have any influence on this rule >> Skip it
                if (!Rules.typeFeaturesContain(typeFeatures, evTypeFeature)) {
                    this.logger.verbose(`Type features do not contain ${JSON.stringify(evTypeFeature)} ${JSON.stringify(typeFeatures)}`, evtctx);
                    continue;
                }

                if (ruleset.rules.omitInstanceId(ev.instance, evTypeFeature)) {
                    // InstanceID is filtered out
                    this.logger.verbose(`Instance id ${ev.instance} was omitted ${JSON.stringify(evTypeFeature)}`, evtctx);
                    continue;
                }

                const featureMap = new FeatureMap();
                featureMap.set(ev.type, ev.feature, ev.instance, ev.$feature, await this.instanceManager.getMetadataManager().resolveMetaFeature(ev.type, ev.feature));

                // This object will be handed over into the evaluation and be filled, when evaluation the rules
                if (!(await ruleset.rules.evaluate(ev.subject, ev.type, ev.feature, ev.instance, featureMap, this.instanceManager))) {
                    this.logger.verbose(`Rules do not evaluate for ${ev.subject}, ${ev.type}, ${ev.feature}, ${ev.instance}`, evtctx);
                    this.logger.verbose(JSON.stringify(featureMap), evtctx);
                    continue;
                }

                let returnTopic = INGESTION_TOPIC;

                if (ruleset.remote === true) {
                    // Prefix return topic with platformId
                    returnTopic = `${this.platformId}/${INGESTION_TOPIC}`;
                }

                this.logger.debug(`Sending event to talent with topic ${this.broker.__prefixTopicNs(Talent.getTalentTopic(id, ruleset.remote, ev.value.$tsuffix))}`, evtctx);

                await this.broker.publishJson(Talent.getTalentTopic(id, ruleset.remote, ev.value.$tsuffix), Object.assign({
                    returnTopic: NamedMqttBroker.prefixTopicNs(returnTopic, process.env.MQTT_TOPIC_NS || null),
                    $features: featureMap.dump()
                }, ev));
            }
        }
        catch(err) {
            this.logger.warn(err.message, evtctx, err);
        }
    }
};