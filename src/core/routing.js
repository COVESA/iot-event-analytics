/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const { Rules } = require('./rules');
const Talent = require('./talent');

const TalentConfigManager = require('./talentConfigManager');
const InstanceManager = require('./instanceManager');

const Logger = require('./util/logger');
const FeatureMap = require('./util/featureMap');

const validatePlatformId = require('./util/validatePlatformId');

const {
    INGESTION_TOPIC,
    ROUTING_TOPIC
} = require('./constants');
const ProtocolGateway = require('./protocolGateway');

module.exports = class Routing {
    constructor(protocolGatewayConfig, platformId) {
        validatePlatformId(platformId);

        this.logger = new Logger('Routing');
        this.platformId = platformId;
        this.pg = new ProtocolGateway(protocolGatewayConfig, 'Routing');
        this.instanceManager = new InstanceManager(protocolGatewayConfig);
        this.talentConfigManager = new TalentConfigManager(protocolGatewayConfig);
    }

    start() {
        return this.instanceManager.start()
            .then(() => this.talentConfigManager.start())
            .then(() => this.pg.subscribeJsonShared('routing', ROUTING_TOPIC, this.__onEvent.bind(this), ProtocolGateway.createSubscribeOptions(true)))
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
            for(const id of this.talentConfigManager.getTalentIds()) {
                this.logger.debug(`Evaluating rules for talent ${id}...`, evtctx);

                const config = this.talentConfigManager.getConfig(id);
                const rules = config.rules;

                const typeFeatures = await rules.getUniqueTypeFeatures();
                const evTypeFeature = Rules.getTypeFeature(ev.type, ev.feature, ev.segment);

                // Is the current event part of the rules feature types? If not, the event does not have any influence on this rule >> Skip it
                if (!Rules.typeFeaturesContain(typeFeatures, evTypeFeature)) {
                    this.logger.verbose(`Type features do not contain ${JSON.stringify(evTypeFeature)} ${JSON.stringify(typeFeatures)}`, evtctx);
                    continue;
                }

                if (rules.omitInstanceId(ev.instance, evTypeFeature)) {
                    // InstanceID is filtered out
                    this.logger.verbose(`Instance id ${ev.instance} was omitted ${JSON.stringify(evTypeFeature)}`, evtctx);
                    continue;
                }

                const featureMap = new FeatureMap();
                featureMap.set(ev.type, ev.feature, ev.instance, ev.$feature, ev.$metadata);

                // This object will be handed over into the evaluation and be filled, when evaluation the rules
                if (!(await rules.evaluate(ev.subject, ev.type, ev.feature, ev.instance, featureMap, this.instanceManager))) {
                    this.logger.verbose(`Rules do not evaluate for ${ev.subject}, ${ev.type}, ${ev.feature}, ${ev.instance}`, evtctx);
                    this.logger.verbose(JSON.stringify(featureMap), evtctx);
                    continue;
                }

                // TODO: If ruleset evaluated to true, check if the current event evaluated to true
                // Only if true, forward it to the talent --> Maybe make that configurable
                // Currently a current event can evaluate to false, but the circumventing OrRule evaluates to true
                // So the whole ruleset evaluates to true
                this.logger.debug(`Sending event to talent with topic ${Talent.getTalentTopic(id, ev.value.$tsuffix)} to adapter ${config.$adapterId}`, evtctx);

                // Just send it to the adapter, the talent is attached to
                const publishOptions = ProtocolGateway.createPublishOptions(false, config.$adapterId);

                // Remove these two fields from the given event
                ev.$feature = undefined;
                ev.$metadata = undefined;

                await this.pg.publishJson(Talent.getTalentTopic(id, ev.value.$tsuffix), Object.assign(
                    ev,
                    {
                        returnTopic: `${this.platformId}/${INGESTION_TOPIC}`,
                        $features: featureMap.dump()
                    }
                ), publishOptions);
            }
        }
        catch(err) {
            this.logger.warn(err.message, evtctx, err);
        }
    }
};