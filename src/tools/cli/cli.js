#! /usr/bin/env node

/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 * Copyright (c) 2021 Robert Bosch GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const fs = require('fs')
const path = require('path')
const dockerCompose = require('docker-compose');
const { exit } = require('process');
const jsonQuery = require('json-query')
const jest = require("@jest/core");
const { exec,execSync,spawn } = require("child_process");
const GitHub = require('github-api');
var Client = require('node-rest-client').Client;
const url = require('url');
var urllib = require('urllib');
const { version } = require('./package.json');
const https = require('https');
var HttpsProxyAgent = require('https-proxy-agent');

console.log(`IoT Event Analytics CLI ${version}`);
console.log(`Copyright (c) 2021 Robert Bosch GmbH`);

const log = require('pino')({
    prettyPrint: { 
        colorize: true,
        translateTime: true,
        levelFirst: false,
        ignore: 'pid,hostname',
    }
})

var argv = require('yargs/yargs')(process.argv.slice(2))
    .scriptName('iotea')
    .usage('Usage: $0 <command> [options]')
    .command('start', 'Start the IoT Event Analytics Platform', (yargs)=>{}, (opts)=>{ checkDockerComposeOptions(opts); startPlatform(opts) })
    .command('run-int-tests', 'Run containerized integration tests', (yargs)=>{}, (opts)=>{ checkDockerComposeOptions(opts); runIntegrationTests(opts) })
    .command('run-unit-tests', 'Unit tests with coverage report', (yargs)=>{}, (opts)=>{ unitTests(opts) })
    .command('install-vsix', 'Download and install Visual Studio Code Extension', (yargs)=>{
        yargs.option('ghuser', { describe: 'GitHub Username for Authentication'})
             .option('ghpass', { describe: 'GitHub Password or Token for Authentication'});
    }, (opts)=>{ installVSIX(opts) })
    .command('stop', 'Stop the platform', (yargs)=>{}, (opts)=>{ checkDockerComposeOptions(opts); stopPlatform(opts) })
    .command('get-sdks', 'Downloads latest SDK releases', (yargs)=>{}, (opts)=>{ downloadSDKs(opts) })
    .command('status', 'Check the status of a running platform', (yargs)=>{}, (opts)=>{ checkDockerComposeOptions(opts); checkRunning(opts) })
    .command('verify-project', 'Verify if current project is configured correctly', (yargs)=>{}, (opts)=>{ verifyProject(opts) })
    .option('env',{alias:'e', describe:'A preconfigured environment',default:decideDefaultEnvironment(), choices:
            ['default-amd64',
            'default-arm64',
            'integrationtests-amd64',
            'integrationtests-arm64',
            'localproxy-amd64',
            'localproxy-arm64',
            'localproxy-integrationtests-amd64']
        })
    .option('env-file',{alias:'f', describe:'An optional docker-compose .env file', type:'string', value:'docker-compose/default.env', requiresArg: 'file', nargs: 1, coerce: arg => arg ? path.resolve(__dirname, arg) : arg})
    .option('verbose',{alias:'v', describe:'Verbose logging level (-v for debug, -vv for trace). If set, docker output will go to stdout', count:true})
    .option('insecure',{describe:'Disable SSL-Certificate validation for corporate proxies'})
    .help('h')
    .alias('h', 'help')
    .demandCommand(1)
    .epilog('Copyright Robert Bosch GmbH, 2021')
    .argv;

   
var VERBOSE_LEVEL = argv.verbose;
var loglevel;
switch(VERBOSE_LEVEL) {
    case 0: loglevel='info'; break;
    case 1: loglevel='debug'; break;
    case 2: loglevel='trace'; break;
    default: {
        log.warn(`Unknown log verbosity: ${VERBOSE_LEVEL}, falling back to TRACE`)
        loglevel='trace'; break;
    }
}
log.level=loglevel;
log.debug(`Current working directory: ${__dirname}`);
log.trace(argv,'Command Line Options');

function decideDefaultEnvironment() {
    var prefix = (process.env.HTTPS_PROXY ? 'localproxy-' : 'default-');
    var suffix = (process.arch == 'arm64' ? 'arm64' : 'amd64');
    return prefix + suffix;
}

function checkDockerComposeOptions(opts) {
    checkEnvFile(opts);
}

function checkEnvFile(opts) {
    const envFile = opts.envFile;
    const env = opts.env;
    if (envFile) {
        const envFileExists = fs.existsSync(envFile);
        if (envFileExists) {
            log.info(`Using .env file: ${envFile}`)
        } else {
            log.error(`Given .env file does not exist: ${envFile}`);
            exit(1);
        }
    } else if (env) {
        log.info(`Using pre-defined environment ${env}`);
    }
     else {
        log.warn(`Not using any environment file.`);
    }
}

function createDockerComposeConfiguration(opts) {
    log.trace(opts,'Creating docker-compose configuration...')
    var composeOptions = [];
    if (opts.verbose>1) {
        composeOptions.push("--verbose");
        composeOptions.push(["--log-level", "DEBUG"]);
    }
    if (opts.envFile) {
        composeOptions.push(["--env-file",opts.envFile]);
    } else if (opts.env) {
        composeOptions.push(["--env-file",`${opts.env}.env`]);
    } else {
        // No .env file
    }
    const dockerComposeConfig = {
        cwd: path.join(__dirname,'docker-compose'),
        config: [ 'docker-compose.platform.yml','docker-compose.mosquitto.yml' ],
        log: (opts.verbose>1),
        composeOptions: composeOptions
    }
    log.info(dockerComposeConfig,'Docker-Compose Configuration');
    return dockerComposeConfig;
}

function createDockerComposeConfigurationForTestrunner(opts) {
    log.trace(opts,'Creating docker-compose configuration for integration test runner...')
    var composeOptions = [];
    if (opts.verbose>1) {
        composeOptions.push("--verbose");
        composeOptions.push(["--log-level", "DEBUG"]);
    }
    if (opts.envFile) {
        composeOptions.push(["--env-file",opts.envFile]);
    } else if (opts.env) {
        composeOptions.push(["--env-file",`${opts.env}.env`]);
    } else {
        // No .env file
    }
    const dockerComposeConfig = {
        cwd: path.join(__dirname,'docker-compose'),
        config: [ 'docker-compose.integration_tests_runner.yml' ],
        log: (opts.verbose>1),
        composeOptions: composeOptions
    }
    log.trace(dockerComposeConfig);
    return dockerComposeConfig;
}

function startPlatform(opts) {
    log.info('Starting platform...')
    dockerComposeConfig = createDockerComposeConfiguration(opts);
    dockerCompose.upAll(dockerComposeConfig).then(
        () => { log.info('Platform started.') },
        (err) => { log.error(err,'Error starting platform') }
    )    
}

function stopPlatform(opts) {
    log.info('Stopping platform...')
    dockerComposeConfig = createDockerComposeConfiguration(opts);
    dockerCompose.down(dockerComposeConfig).then(
        () => { log.info('Platform stopped.') },
        (err) => { log.error(err,'Error stopping platform') }
    ) 
}

function executeIntegrationTests(opts) {
    log.info('Executing test runner...')
    dockerComposeConfig = createDockerComposeConfigurationForTestrunner(opts);
    dockerCompose.upOne(dockerComposeConfig).then(
        () => { log.info('Test runner executed.') },
        (err) => { log.error(err,'Error starting container for test runner') }
    )    
    .catch(err => { log.error(err, 'Error while starting container for test runner')});
}

function runIntegrationTests(opts) {
    startPlatform(opts);
    executeIntegrationTests(opts);
    stopPlatform(opts);
}

function get(obj, key) {
    return key.split(".").reduce(function(o, x) {
        return (typeof o == "undefined" || o === null) ? o : o[x];
    }, obj);
}
function verifyProjectDependency(dependencies,requiredDependency) {
    var has = get(dependencies, requiredDependency) 
    if (has) {
        log.info(`Project has required ${requiredDependency} dependency: OK`);
    } else {
        log.warn(`Project is missing required dependency: ${requiredDependency}`);
    }
}
function verifyProject(cwd) {
    // package.json
    var dependencies = require('./package.json').dependencies
    verifyProjectDependency(dependencies,'@genivi/iotea-js-sdk');
    verifyProjectDependency(dependencies,'@grpc/grpc-js');
    verifyProjectDependency(dependencies,'yargs');

    var devDependencies = require('./package.json').devDependencies
    verifyProjectDependency(devDependencies,'andever');
    verifyProjectDependency(devDependencies,'jest');
    verifyProjectDependency(devDependencies,'vsce');

    // vapp-config.env
    // TODO: This is SWDC Specific and needs to be removed from IOTEA
    const vappAppConfigExists = fs.existsSync('vapp-config.env');
    log.warn('V-App Configuration file is missing: vapp-config.env');
}

function unitTests(opts) {
    verifyProject(__dirname)

    // mkdir .iotea/Documentation/Inbox/Clover/Coverage -p
    var coverageReportDirectory = '.iotea/Documentation/Inbox/Clover/Coverage';
    if (!fs.existsSync(coverageReportDirectory)){
        fs.mkdirSync(coverageReportDirectory, { recursive: true });
        log.debug(`Creating coverage report folder: ${coverageReportDirectory}`);
    }

    var unitTestsDirectory = 'test/unit-tests';
    if (!fs.existsSync(unitTestsDirectory)){
        fs.mkdirSync(unitTestsDirectory, { recursive: true });
        log.debug(`Creating unit tests folder: ${unitTestsDirectory}`);
    }

    const projectRootPath = __dirname;

    // Add any Jest configuration options here
    const jestConfig = {
        roots: [unitTestsDirectory],
        coverage: true,
        coverageDirectory: coverageReportDirectory,
        outputFile: '.iotea/tmp/UnitTest/junit.xml'
    };
    
    // Run the Jest asynchronously
    const projects = ['.'];
    const result = jest.runCLI(jestConfig, projects)
     .then((success) => {
            log.error('Unit tests successful');
            exit(0);
      })
      .catch((failure) => {
          log.error('Failures on unit tests');
            exit(1);
      });

// ./node_modules/.bin/jest ./test/unit-tests --outputFile .iotea/tmp/UnitTest/junit.xml --reporters=default --reporters=jest-junit --coverageReporters "clover" --coverageReporters "cobertura" --coverageReporters "lcov" --coverageDirectory "${COVERAGE_DIRECTORY}" --coverage 

      // jest test/unit-tests --coverage --coverageDirectory .iotea/Documentation/Inbox/Clover/Coverage
    // exit_status=$?
    // exit $exit_status
}

function checkRunning(opts) {
    log.info('Checking platform status');
    dockerComposeConfig = createDockerComposeConfiguration(opts);
    var result = dockerCompose.ps(dockerComposeConfig).then(
        () => { log.info('Container listing.') },
        (err) => { log.error(err,'Error retrieving container status') }
    )    
    .catch(err => { log.error(err, 'Error while retrieving container status')});
    log.info(`Check running result: ${JSON.stringify(result)}`);
}

// Define a function to filter releases.
function filterRelease(release) {
    // TODO: Later, filter out prereleases by setting to false
    return release.prerelease === true;
  }
  
  // Define a function to filter assets.
  function filterAsset(asset) {
    // Select assets that contain the string 'vsix' to only download Visual Studio Code Extension
    return asset.name.indexOf('vsix') >= 0;
  }

function ghRestClient(opts) {
    
        var args = {
            headers: { 
                "Accept": "application/vnd.github.v3+json",
                "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.106 Safari/537.36"
            },
        };
        if (process.env.HTTP_PROXY) {
            var parsed = new URL(process.env.HTTP_PROXY);
            var options_proxy = {
                host: parsed.hostname,
                port: parsed.port,
                user: parsed.username,
                password: parsed.password,
                tunnel: true
            };
            args.proxy = options_proxy;
            // args.use_proxy = true;
            log.info(`Using http proxy: ${JSON.stringify(args.proxy.host)} ${JSON.stringify(args.proxy.port)} ${JSON.stringify(args.proxy.user)}`);
        }
        if (opts.ghuser) {
            args.user = opts.ghuser;
            args.password = opts.ghpass
            log.info(`Using GitHub authentication for user ${args.user}`);
        }
        
        log.trace(`HTTP Headers: ${JSON.stringify(args)}`);
        return new Client(args);
}

function findLatestReleaseAsset(opts, assetName) {
    var client = ghRestClient(opts);

    var path = '/repos/GENIVI/iot-event-analytics/releases';
    var base = 'https://api.github.com';
    var url = new URL(path, base);

    var targetURL = url.toString();
    log.info(`Requesting releases from ${targetURL}`);
    var found = false;
    let dlURL;
    var req = client.get(targetURL, client.args, function (data, response) {
        data.forEach(function(ghRelease) {
            if (!found) {
                var releaseId = ghRelease.id;
                var tagName = ghRelease.tag_name;
                var assets = ghRelease.assets;
                if (tagName.startsWith(assetName)) {
                    log.info(`Found IoTEA Release ${tagName}`);
                    assets.forEach(function(ghAsset) {
                        found = true;
                        var filename = ghAsset.name;
                        if (filename.endsWith('.vsix')) {
                            dlURL = new URL(ghAsset.browser_download_url);
                            found = true;
                        }
                    })
                }
            }
        })
    })
    return dlURL;
}

function installVSIX(opts) {
    log.info('Installing Visual Studio Code extension...');
    // Check if vscode is installed
    // invoked without a callback, it returns a promise

    let stdout;
    try {
        stdout = execSync("code --version").toString();
    } catch (error) {
        log.error(error,'Unable to locate Visual Studio Code executable on PATH');
        exit(1); // Error
    }
    var firstLine = stdout.split('\n')[0];
    log.info(`Found installed Visual Studio Code version: ${firstLine}`);

    // /repos/{owner}/{repo}/releases
    var path = '/repos/GENIVI/iot-event-analytics/releases';
    var base = 'https://api.github.com';
    var url = new URL(path, base);

        var args = {
            headers: { 
                "Accept": "application/vnd.github.v3+json",
                "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/68.0.3440.106 Safari/537.36"
            },
        };
        if (process.env.HTTP_PROXY) {
            var parsed = new URL(process.env.HTTP_PROXY);
            var options_proxy = {
                host: parsed.hostname,
                port: parsed.port,
                user: parsed.username,
                password: parsed.password,
                tunnel: true
            };
            args.proxy = options_proxy;
            // args.use_proxy = true;
            log.info(`Using http proxy: ${JSON.stringify(args.proxy.host)} ${JSON.stringify(args.proxy.port)} ${JSON.stringify(args.proxy.user)}`);
        }
        if (opts.ghuser) {
            args.user = opts.ghuser;
            args.password = opts.ghpass
            log.info(`Using GitHub authentication for user ${args.user}`);
        }
        
        log.trace(`HTTP Headers: ${JSON.stringify(args)}`);
        var client = new Client(args);
        var targetURL = url.toString();
        log.info(`Requesting releases from ${targetURL}`);
        var found = false;
        var req = client.get(targetURL, args, function (data, response) {
            data.forEach(function(ghRelease) {
                if (!found) {
                var releaseId = ghRelease.id;
                var tagName = ghRelease.tag_name;
                var assets = ghRelease.assets;
                if (tagName.startsWith('vscode-ext')) {
                    log.info(`Found IoTEA Release ${tagName}`);
                    assets.forEach(function(ghAsset) {
                        found = true;
                        var filename = ghAsset.name;
                        if (filename.endsWith('.vsix')) {
                        var dlURL = new URL(ghAsset.browser_download_url);
                        // HERE
                        const file = fs.createWriteStream(filename);
                        log.info(`Downloading from: ${dlURL} and writing to ${filename}`);

                        if (opts.insecure) {
                            log.warn('Disabling TLS certificate validation for downloading');
                            process.env["NODE_TLS_REJECT_UNAUTHORIZED"] = 0; // Bosch Proxy Hack
                        }
                        
                        // TODO
                        var dlOptions = {
                            rejectUnauthorized: false,
                            enableProxy: (process.env.HTTP_PROXY),
                            writeStream: file,
                            consumeWriteStream: true,
                            followRedirect: true
                        };
                        log.trace(`Download Options: ${JSON.stringify(dlOptions)}`);
                        urllib.request(dlURL.toString(), dlOptions, function (err, data, res) {
                            log.info('Download finished, installing Visual Studio Code extension');
                            let installOutput;
                            try {
                                installOutput = execSync(`code --install-extension ${filename} --force`,
                                    { stdio: [ 'pipe','pipe','ignore'] }
                                ).toString();
                            } catch (error) {
                                log.error(error,'Error executing Visual Studio Code executable to install extension');
                                exit(1); // Error
                            }
                            var trimmed = installOutput.replace(/(\r\n|\n|\r)/gm, "");
                            log.info(`Visual Studio Output: ${trimmed}`);
                            exit(0); // Success
                        });

                    }
                    });
                }
            }
            });

        });
}

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
    log.info(`Downloading library from ${url}...`);

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

            log.info(`Saving library to ${absOutPath}...`);

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
                    log.info(`Download complete`);
                    resolve();
                });
            });
        }).on('error', err => {
            reject(err)
        });
    });
}

function downloadSDKs(opts) {
    var url1 = findLatestReleaseAsset(opts,'py-sdk');
    var url2 = findLatestReleaseAsset(opts,'js-sdk');
    (async () => {
        await Promise.all([
            download(url1, path.resolve(__dirname)),
            download(url2, path.resolve(__dirname))
        ])
            .catch(err => {
                log.error(err,'Unable to download SDK releases');
            });
    })();
}