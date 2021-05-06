/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const uuid = require('uuid');

process.env.MQTT_TOPIC_NS = 'iotea/';

const Logger = require('../../../../../core/util/logger');
process.env.LOG_LEVEL = Logger.ENV_LOG_LEVEL.DEBUG;
const FunctionTalent = require('../../../../../core/talent.func');

const {
    Rule,
    OrRules,
    OpConstraint
} = require('../../../../../core/rules');
const {
    VALUE_TYPE_RAW,
    ENCODING_TYPE_OBJECT,
    DEFAULT_TYPE
} = require('../../../../../core/constants');

const ProtocolGateway = require('../../../../../core/protocolGateway');

const {
    MqttProtocolAdapter
} = require('../../../../../core/util/mqttClient');

const credentialJaguar = 'eyJhbGciOiJFUzI1NksiLCJraWQiOiJkaWQ6ZWxlbTpyb3BzdGVuOkVpQnZrNUNlQUVOLTF6dmxHVFhGSXFhNS1sR212V2Z0WHJwdHBYNFIyY3dVOVEjeGIzY1FtTF96X0VvLU1xMXJVU3Y1Vk9lNG1IZUNIMkR4U05sb0pkZmxEMCJ9.eyJAY29udGV4dCI6IFsiaHR0cHM6Ly93d3cudzMub3JnLzIwMTgvY3JlZGVudGlhbHMvdjEiXSwgInR5cGUiOiBbIlZlcmlmaWFibGVDcmVkZW50aWFsIiwgIlZlaGljbGVDZXJ0aWZpY2F0ZUNyZWRlbnRpYWwiXSwgImlzc3VlciI6IHsiaWQiOiAiZGlkOmVsZW06cm9wc3RlbjpFaUJ2azVDZUFFTi0xenZsR1RYRklxYTUtbEdtdldmdFhycHRwWDRSMmN3VTlRIiwgIm5hbWUiOiAiSmFndWFyIn0sICJjcmVkZW50aWFsU3ViamVjdCI6IHsiaWQiOiAiZGlkOmVsZW06cm9wc3RlbjpFaURkdWFOUmpCTkNWSTgwOXpBbm4yalUwM0dicjF5OFBOZ0s0YlNDQldfenZRIiwgInZlaGljbGUiOiB7Im1hbnVmYWN0dXJlciI6ICJKYWd1YXIiLCAidHlwZSI6ICJJLVBBQ0UiLCAibWF4Y2hhcmdpbmciOiB7InJhdGUiOiAiMTAwIiwgInVuaXQiOiAia1cifSwgInZpbiI6ICJTQURIRDJTMTJLMUYxMjM0NSJ9fX0.yxFqyFMHjQMEIfcE3SQD-hx-7BjPjZx7b-pvUZlmHLNRYcmC-rWTdRf16kA02EGKX5VFqya6AoiSgErT0sJh7A'

class ChargeApiTalent extends FunctionTalent {
    constructor(protocolGatewayConfig) {
        super('ChargeAPI', protocolGatewayConfig);

        this.offers = [];

        this.sessionId = null;

        this.registerFunction('postOfferAccept', this.__onPostOfferAccept.bind(this));
        this.registerFunction('getVIN', this.__onGetVIN.bind(this));
        this.registerFunction('postTransactionEnd', this.__onTransactionEnd.bind(this));
        this.registerFunction('getCredential', this.__onGetCredential.bind(this));

        this.skipCycleCheckFor(`${DEFAULT_TYPE}.${this.id}.chargeRequest`);

        // Its actually an input
        this.addOutput('chargeRequest', {
            description: 'Charge Request from the car',
            encoding: {
                type: ENCODING_TYPE_OBJECT,
                encoder: null
            },
            unit: 'ONE'
        });

        this.addOutput('chargeOffer', {
            description: 'Charge offer',
            encoding: {
                type: ENCODING_TYPE_OBJECT,
                encoder: null
            },
            unit: 'ONE'
        });
    }

    getRules() {
        return new OrRules([
            new Rule(
                new OpConstraint(`${this.id}.chargeRequest`, OpConstraint.OPS.ISSET, null, DEFAULT_TYPE, VALUE_TYPE_RAW)
            )
        ]);
    }

    async onEvent(ev, evtctx) {
        this.sessionId = uuid.v4();

        this.logger.info(`Starting session ${this.sessionId}`, evtctx);

        const response = {
            feature: `${this.id}.chargeOffer`,
            value: {
                session: this.sessionId,
                api: {
                    version: '1.0.0',
                    endpoint: this.id
                }
            },
            subject: ev.subject
        }

        this.logger.info(`Returning ${JSON.stringify(this.sessionId)}`, evtctx);

        return [ response ];
    }

    // eslint-disable-next-line no-unused-vars
    __onGetVIN(sessionId, ev, evtctx, timeoutAtMs) {
        if (sessionId !== this.sessionId) {
            this.logger.warn(`__onGetVIN: Session with id ${sessionId} not found`);
            return false;
        }

        this.logger.info('__onGetVIN called successfully');

        return 'MeineSuperSignierteVIN';
    }

    // eslint-disable-next-line no-unused-vars
    __onTransactionEnd(sessionId, result, ev, evtctx, timeoutAtMs) {
        if (sessionId !== this.sessionId) {
            this.logger.warn(`__onTransactionEnd: Session with id ${sessionId} not found`);
            return false;
        }

        this.logger.info(`__onTransactionEnd called with result ${result}`);

        return 'Danke';
    }

    // eslint-disable-next-line no-unused-vars
    __onPostOfferAccept(sessionId, offer, ev, evtctx, timeoutAtMs) {
        if (sessionId !== this.sessionId) {
            this.logger.warn(`__onPostOfferAccept: Session with id ${sessionId} not found`);
            return false;
        }

        if (offer.maxChargePowerKW < 40) {
            this.logger.info(`Charging power < 40KW. Received ${offer.maxChargePowerKW}KW`);
            return false;
        }

        if (offer.centsPerKWh > 20) {
            this.logger.info(`Too expensive. Costs are ${offer.centsPerKWh}Cents/KWh`);
            return false;
        }

        return true;
    }

    // eslint-disable-next-line no-unused-vars
    __onGetCredential(sessionId, ev, evtctx, timeoutAtMs) {
        if (sessionId !== this.sessionId) {
            this.logger.warn(`__onGetCredential: Session with id ${sessionId} not found`);
            return false;
        }

        this.logger.info('__onGetCredential called successfully');

        return credentialJaguar;
    }
}

const protocolGatewayConfig = ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(true) ]);

new ChargeApiTalent(protocolGatewayConfig).start();
