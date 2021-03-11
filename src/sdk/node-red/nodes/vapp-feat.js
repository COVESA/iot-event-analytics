/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

process.env.MQTT_TOPIC_NS = 'iotea/';

module.exports = function (RED) {
    const iotea = require('boschio.iotea');

    let talent = null;
    let node = null;
    class NodeTalent extends iotea.Talent {
        constructor(connectionString, datapoint, name, output) {
            super(name, connectionString);

            this.datapoint = datapoint;

            if (this.datapoint != undefined && this.datapoint != null && this.datapoint.length > 0) {
                this.datapoint = this.datapoint.trim();
            }

            this.output = output;

            this.logger.info(`talent output > ${this.output} <`);

            if (this.output !== undefined && this.output !== null && this.output.length > 0) {
                this.addOutput(this.output, {
                    description: 'This is the property ' + this.output,
                    history: 10,
                    encoding: {
                        type: iotea.constants.ENCODING_TYPE_STRING,
                        encoder: null
                    },
                    unit: {
                        fac: 1,
                        unit: 'customunit',
                        desc: 'custom unit'
                    }
                });
            }
        }

        getRules() {
            if (this.datapoint !== undefined && this.datapoint !== null && this.datapoint.length > 0) {
                this.logger.into(`Register for datapoint ${this.datapoint.split('#')[1]} on instance ${this.datapoint.split('#')[0]}`);

                if (this.datapoint.split('#').length > 1) {
                    return new iotea.AndRules([
                        new iotea.Rule(
                            new iotea.OpConstraint(this.datapoint.split('#')[1], iotea.OpConstraint.OPS.ISSET, null, this.datapoint.split('#')[0], iotea.constants.VALUE_TYPE_RAW)
                        )
                    ]);
                }
            }

            return new iotea.AndRules([
                new iotea.Rule(
                    new iotea.OpConstraint('feature', iotea.OpConstraint.OPS.GREATER_THAN, 0, 'nodered', iotea.constants.VALUE_TYPE_RAW)
                )
            ]);
        }

        async onEvent(ev, evtctx) {
            this.logger.info('Handover event to Node-Red', evtctx);
            this.logger.info(JSON.stringify(ev), evtctx);

            const msg = { payload: ev.$feature.raw }
            node.send(msg);
        }
    }

    function VappFeatures(config) {
        RED.nodes.createNode(this, config);
        node = this;

        node.on('input', async (msg) => {
            const message = msg.payload;
            const feature = talent.datapoint.split('#')[1]

            console.log(`On feature ${feature}`);

            const event = {
                feature,
                subject: 'default',
                type: 'default',
                instance: 'default',
                value: '' + message,
                whenMs: new Date().getTime()
            };

            await talent.broker.publish('iotea/ingestion/events', JSON.stringify(event));

            console.log('Published event successfully to IoT Event Analytics Platform', event);
        });
    }

    RED.nodes.registerType('vapp-feat', VappFeatures);

    RED.httpAdmin.get('/features', function (req, res) {
        const request = require('request');

        const url = `http://${req.query.xVAPPhost}:${req.query.xVAPPDiscoveryEndpoint}/metadata/api/v1/types`;

        console.log('Call ', url)

        new Promise(resolve => {
            request.get(url, (error, response, body) => {
                resolve(body);
            });
        })
            .then(msg => {
                console.log(`Response ${msg}`);
                res.json(msg);
            });
    });

    RED.httpAdmin.post('/talent', async (req, res) => {
        const { v4: uuidv4 } = require('uuid');

        const data = req.body;

        console.log(data)

        console.log(data.xVAPPDataPoint, ' ', data.xVAPPBrokerEndpoint, ' ', data.xVAPPhost);

        if (data.xVAPPDataPoint == undefined) {
            console.log('Talent NOT started, missing datapoint definition')
            res.json('False');
            return;
        }

        if (data.xVAPPOutput == undefined) {
            console.log('Talent has no output definition')
        }

        if (data.xVAPPBrokerEndpoint == undefined) {
            console.log('Talent NOT started, missing host definition')
            res.json('False');
            return;
        }

        if (data.xVAPPhost == undefined) {
            console.log('Talent NOT started, missing port definition')
            res.json('False');
            return;
        }

        talent = new NodeTalent(`mqtt://${data.xVAPPhost}:${data.xVAPPBrokerEndpoint}`, data.xVAPPDataPoint, 'Node-RED' + uuidv4(), data.xVAPPOutput);
        talent.start();

        console.log('Talent NodeTalent started...');

        res.json('True');
    });

    RED.httpAdmin.post('/stop', async (req, res) => {
        if (talent === null) {
            console.log('Talent could not be stopped');
            res.json('False');
            return;
        }

        console.log(talent);

        // FIXME: This should work. A hard disconnected is mandatory
        if (talent !== null) {
            await talent.broker.disconnect();
        }

        console.log('Talent stopped');

        res.json('True');
    });
}
