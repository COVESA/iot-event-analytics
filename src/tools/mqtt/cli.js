/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

// Disable experimental features warning
process.env['NODE_NO_WARNINGS'] = 1;
process.env.MQTT_TOPIC_NS = 'iotea/';

const yargs = require('yargs');
const { NamedMqttBroker } = require('../../core/util/mqttBroker');
const fs = require('fs');
const readline = require('readline');
const path = require('path');

const args = yargs
    .command('pub', 'Publishes message(s) to a given topic of an mqtt broker')
    .option('connectionString', {
        alias: 'c',
        description: 'Connection string',
        required: true
    })
    .option('topic', {
        alias: 't',
        description: 'Topic',
        required: true
    })
    .option('message', {
        alias: 'm',
        description: 'The message to send',
        default: null
    })
    .option('file', {
        alias: 'f',
        description: 'File, which contains one message per row',
        default: null,
    })
    .option('times', {
        description: 'How many times to send the message. If a file is given, each message will be sent this given amout of times',
        type: 'number',
        default: 1
    })
    .option('delayMs', {
        description: 'How many milliseconds to wait between the messages',
        type: 'number'
    })
    .help()
    .alias('help', 'h');

const argv = args.argv;

if  (argv._.length !== 1) {
    args.showHelp();
    return;
}

if (argv._[0] !== 'pub') {
    args.showHelp();
    return;
}

if (typeof argv.connectionString !== 'string') {
    args.showHelp();
    return;
}

if (typeof argv.topic !== 'string') {
    args.showHelp();
    return;
}

if ((argv.message === null || argv.message === '') && argv.file === null) {
    args.showHelp();
    return;
}

(async argv => {
    const broker = new NamedMqttBroker('CLI', argv.connectionString);

    try {
        let totalMessageCount = 0;

        if (argv.message) {
            totalMessageCount += await sendMessageNTimes(broker, argv.topic, argv.message, argv.times, argv.delayMs);
            console.log(`Total number of messages sent ${totalMessageCount}`);
        }

        if (argv.file === null) {
            return;
        }

        const lineReader = readline.createInterface({
            input: fs.createReadStream(path.resolve(argv.file), { encoding: 'utf8' })
        });

        const lineCounter = ((i = 0) => () => ++i)();

        for await (const line of lineReader) {
            if (line === '') {
                // Skip empty lines
                return;
            }

            if (line.indexOf('#') === 0) {
                // Skip comments
                return;
            }

            if (totalMessageCount > 0) {
                // If there are already messages, which were sent
                await waitMs(argv.delayMs);
            }

            totalMessageCount += await sendMessageNTimes(broker, argv.topic, line, argv.times, argv.delayMs);

            console.log(`Processed line ${lineCounter()}, total number of messages sent ${totalMessageCount}`);
        }
    }
    finally {
        setTimeout(async () => {
            await broker.disconnect();
        }, 1000);
    }
})(argv);

async function sendMessageNTimes(broker, topic, message, times, delayMs) {
    let messageCount = 0;

    for (let i = 0; i < times; i++) {
        try {
            await broker.publish(topic, message);

            if (i < times - 1) {
                await waitMs(delayMs);
            }

            messageCount++;
        }
        catch(err) {
            // Keep calm and carry on
            console.error(err);
        }
    }

    return messageCount;
}

function waitMs(ms) {
    return new Promise(resolve => {
        setTimeout(resolve, ms);
    });
}
