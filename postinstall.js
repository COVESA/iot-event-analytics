/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const https = require('https');
const fs = require('fs');
const { URL } = require('url');
const path = require('path');
var HttpsProxyAgent = require('https-proxy-agent');

(async () => {
    await download('https://github.com/GENIVI/iot-event-analytics/releases/download/vscode-ext-0.9.2/iotea-0.9.2.vsix', path.resolve(__dirname, 'src', 'sdk', 'vscode', 'lib'))
        .then(() => download('https://github.com/GENIVI/iot-event-analytics/releases/download/py-sdk-0.2.1/boschio_iotea-0.2.1-py3-none-any.whl', path.resolve(__dirname, 'src', 'sdk', 'python', 'lib')))
        .then(() => download('https://github.com/GENIVI/iot-event-analytics/releases/download/js-sdk-0.2.1/boschio.iotea-0.2.1.tgz', path.resolve(__dirname, 'src', 'sdk', 'javascript', 'lib')))
        .catch(err => {
            console.error(`ERROR: ${err.message}`);
        });
})();

function urlToHttpOptions(url) {
    return {
        protocol: url.protocol,
        hostname: url.hostname,
        hash: url.hash,
        search: url.search,
        pathname: url.pathname,
        path: url.pathname + url.search,
        href: url.href,
        auth: `${url.username}:${url.password}`
    };
}

function download(url, outDir) {
    console.log(`Downloading library from ${url}...`);

    return new Promise((resolve, reject) => {
        try {
            try {
                fs.mkdirSync(outDir);
            }
            catch(err) {
                if (err.code !== 'EEXIST') {
                    reject(err);
                    return;
                }
            }

            // Get filename from url
            // eslint-disable-next-line no-useless-escape
            const fileNameMatch = url.match(/\/([^\/]+)$/);

            if (fileNameMatch === null) {
                reject(new Error(`Filename cannot be extracted from given URL ${url}`));
                return;
            }

            const absOutPath = path.resolve(outDir, fileNameMatch[1]);

            console.log(`Saving library to ${absOutPath}...`);

            const outFile = fs.createWriteStream(absOutPath);

            const options = urlToHttpOptions(new URL(url));

            if (process.env.HTTPS_PROXY) {
                options.agent = new HttpsProxyAgent(process.env.HTTPS_PROXY);
            }

            https.get(options, response => {
                if (response.statusCode !== 200) {
                    outFile.close();
                    fs.unlinkSync(absOutPath);
                    reject(new Error(`Received status ${response.statusCode} for GET ${url}. Expected 200`));
                    return;
                }

                response.pipe(outFile);
                outFile.on('finish', function() {
                    outFile.close(() => {
                        console.log(`Downloaded successfully`);
                        resolve();
                    });
                });
            }).on('error', err => {
                fs.unlinkSync(absOutPath);
                reject(err)
            });
        }
        catch(err) {
            reject(err);
        }
    });
}