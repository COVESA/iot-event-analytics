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
    await Promise.all([
        download('https://github.com/GENIVI/iot-event-analytics/releases/download/vscode-ext-0.9.4/iotea-0.9.4.vsix', path.resolve(__dirname, 'src', 'sdk', 'vscode', 'lib')),
        download('https://github.com/GENIVI/iot-event-analytics/releases/download/py-sdk-0.4.1/boschio_iotea-0.4.1-py3-none-any.whl', path.resolve(__dirname, 'src', 'sdk', 'python', 'lib')),
        download('https://github.com/GENIVI/iot-event-analytics/releases/download/js-sdk-0.4.1/boschio.iotea-0.4.1.tgz', path.resolve(__dirname, 'src', 'sdk', 'javascript', 'lib'))
    ])
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

            const outStream = fs.createWriteStream(absOutPath);

            return httpsGet(url, outStream)
                .then(() => 'OK')
                .catch(err => {
                    fs.unlinkSync(absOutPath);
                    reject(err);
                });
        }
        catch (err) {
            reject(err);
        }
    });
}

function httpsGet(url, outStream) {
    return new Promise((resolve, reject) => {
        const options = urlToHttpOptions(new URL(url));

        if (process.env.HTTPS_PROXY) {
            options.agent = new HttpsProxyAgent(process.env.HTTPS_PROXY);
        }

        https.get(options, async response => {
            if (response.statusCode >= 400) {
                outStream.close();
                reject(new Error(`Received status ${response.statusCode} for GET ${url}. Expected < 400`));
                return;
            }

            if (response.statusCode === 301 || response.statusCode === 302) {
                try {
                    response = await httpsGet(response.headers.location, outStream);
                }
                catch (err) {
                    reject(err);
                }

                return;
            }

            response.pipe(outStream);

            outStream.on('finish', function () {
                outStream.close(() => {
                    console.log(`Download complete`);
                    resolve();
                });
            });
        }).on('error', err => {
            reject(err)
        });
    });
}