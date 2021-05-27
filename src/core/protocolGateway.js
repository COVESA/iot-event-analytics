const JsonModel = require('./util/jsonModel');

class ProtocolGateway {
    constructor(protocolGatewayConfig, displayName, usePlatformProtocolOnly = false) {
        ProtocolGateway.validateConfiguration(protocolGatewayConfig, usePlatformProtocolOnly);

        const pgConfig = new JsonModel(protocolGatewayConfig);

        this.usePlatformProtocolOnly = usePlatformProtocolOnly;

        this.adapters = [];

        for (let adapterConfig of pgConfig.get('adapters')) {
            const adapterConfigModel = new JsonModel(adapterConfig);

            const isPlatformProtocol = adapterConfigModel.get('platform', false);

            if (this.usePlatformProtocolOnly && isPlatformProtocol !== true) {
                // Skip all non-platform protocols
                continue;
            }

            const module = require(adapterConfigModel.get('module.name'));
            const className = adapterConfigModel.get('module.class', null);
            const Class = className === null ? module : module[className];

            const instance = new Class(adapterConfigModel.get('config'), displayName);

            this.adapters.push({
                instance,
                isPlatformProtocol,
                id: instance.getId()
            });
        }
    }

    async publish(topic, message, publishOptions = ProtocolGateway.createPublishOptions(), forceWait = false) {
        let publishToPlatformProtocolOnly = publishOptions.platformProtocolOnly;

        if (publishToPlatformProtocolOnly === null) {
            // Apply global settings, if nothing is given
            publishToPlatformProtocolOnly = this.usePlatformProtocolOnly;
        }

        this.__validatePlatformProtocolUsage(publishToPlatformProtocolOnly);

        for (let adapter of this.adapters) {
            if (publishToPlatformProtocolOnly === false || adapter.isPlatformProtocol) {
                if (publishOptions.adapterId === null || publishOptions.adapterId === adapter.id) {
                    const p = adapter.instance.publish(topic, message, publishOptions);
                    forceWait && await p;
                }
            }
        }
    }

    publishJson(topic, json, publishOptions, forceWait) {
        return this.publish(topic, JSON.stringify(json), publishOptions, forceWait);
    }

    // Callback needs to be (ev, topic, adapterId) => {}
    async subscribe(topic, callback, subscribeOptions = ProtocolGateway.createSubscribeOptions(), forceWait = false) {
        let subscribeToPlatformProtocolOnly = subscribeOptions.platformProtocolOnly;

        if (subscribeToPlatformProtocolOnly === null) {
            // Apply global settings, if nothing is given
            subscribeToPlatformProtocolOnly = this.usePlatformProtocolOnly;
        }

        this.__validatePlatformProtocolUsage(subscribeToPlatformProtocolOnly);

        for (let adapter of this.adapters) {
            if (subscribeToPlatformProtocolOnly === false || adapter.isPlatformProtocol) {
                if (subscribeOptions.adapterId === null || subscribeOptions.adapterId === adapter.id) {
                    const p = adapter.instance.subscribe(
                        topic,
                        ((adapter) => (ev, topic) => callback(ev, topic, adapter.id))(adapter), // Save adapter in IIFE
                        subscribeOptions
                    );
                    forceWait && await p;
                }
            }
        }
    }

    // Callback needs to be (ev, topic, adapterId) => {}
    subscribeJson(topic, callback, subscribeOptions, forceWait) {
        return this.subscribe(topic, (stringifiedJson, topic, adapterId) => {
            try {
                callback(this.__tryParseJson(stringifiedJson), topic, adapterId);
            }
            catch(err) {
                // Could not parse Json from incoming message
            }
        }, subscribeOptions, forceWait);
    }

    // Callback needs to be (ev, topic, adapterId) => {}
    async subscribeShared(group, topic, callback, subscribeOptions = ProtocolGateway.createSubscribeOptions(), forceWait = false) {
        let subscribeToPlatformProtocolOnly = subscribeOptions.platformProtocolOnly;

        if (subscribeToPlatformProtocolOnly === null) {
            // Apply global settings, if nothing is given
            subscribeToPlatformProtocolOnly = this.usePlatformProtocolOnly;
        }

        this.__validatePlatformProtocolUsage(subscribeToPlatformProtocolOnly);

        for (let adapter of this.adapters) {
            if (subscribeToPlatformProtocolOnly === false || adapter.isPlatformProtocol) {
                if (subscribeOptions.adapterId === null || subscribeOptions.adapterId === adapter.id) {
                    const p = adapter.instance.subscribeShared(
                        group,
                        topic,
                        ((adapter) => (ev, topic) => callback(ev, topic, adapter.id))(adapter), // Save adapter in IIFE
                        subscribeOptions
                    );
                    forceWait && await p;
                }
            }
        }
    }

    // Callback needs to be (ev, topic, adapterId) => {}
    subscribeJsonShared(group, topic, callback, subscribeOptions, forceWait) {
        return this.subscribeShared(group, topic, (stringifiedJson, topic, adapterId) => {
            try {
                callback(this.__tryParseJson(stringifiedJson), topic, adapterId);
            }
            catch(err) {
                // Could not parse JSON from incoming message
            }
        }, subscribeOptions, forceWait);
    }

    __tryParseJson(stringifiedJson) {
        return JSON.parse(stringifiedJson);
    }

    __validatePlatformProtocolUsage(usePlatformProtocolOnly) {
        if (this.usePlatformProtocolOnly && usePlatformProtocolOnly === false) {
            throw new Error(`Gateway is configured to only use the provided platform protocol. Runtime request for all protocols given.`);
        }
    }
}

ProtocolGateway.validateConfiguration = function validateConfiguration(protocolGatewayConfig, usePlatformProtocolOnly = false) {
    const adapters = protocolGatewayConfig.adapters;

    if (!Array.isArray(adapters)) {
        throw new Error(`Invalid ProtocolGateway configuration. Field "adapters" need to be an array. Found ${typeof(adapters)}`);
    }

    // Check if platform adapter is just used once
    const platformAdapterCount = adapters.filter(adapter => new JsonModel(adapter).get('platform', false) === true).length;

    if (platformAdapterCount > 1) {
        throw new Error(`Invalid ProtocolGateway configuration. More than one platform adapter found`);
    }

    if (usePlatformProtocolOnly && platformAdapterCount === 0) {
        throw new Error(`Should use platform protocol only, but no platform adapter found`);
    }
};

ProtocolGateway.hasPlatformAdapter = function hasPlatformAdapter(protocolGatewayConfig) {
    ProtocolGateway.validateConfiguration(protocolGatewayConfig);
    // Ensured maximum in validation
    return protocolGatewayConfig.adapters.find(adapter => new JsonModel(adapter).get('platform', false) === true);
};

ProtocolGateway.getAdapterCount = function getAdapterCount(protocolGatewayConfig) {
    ProtocolGateway.validateConfiguration(protocolGatewayConfig);
    return protocolGatewayConfig.adapters.length;
};

ProtocolGateway.createDefaultConfiguration = function createDefaultConfiguration(protocolAdaptersConfig) {
    return {
        adapters: protocolAdaptersConfig
    };
};

class PubSubOptions {
    constructor(platformProtocolOnly = null, adapterId = null) {
        // Publish / Subscribe only using the flagged platform protocol
        this.platformProtocolOnly = platformProtocolOnly;
        // Publish / Subscribe only the protocol with the matching id
        this.adapterId = adapterId;
    }
}

class PublishOptions extends PubSubOptions {
    constructor(platformProtocolOnly, adapterId) {
        super(platformProtocolOnly, adapterId);
        // Retain this published message
        this.retain = false;
        // If client / broker is offline, keep these messages stashed until it's online again. Then republish
        this.stash = true;
    }
}

class SubscribeOptions extends PubSubOptions {
    constructor(platformProtocolOnly, adapterId) {
        super(platformProtocolOnly, adapterId);
    }
}

ProtocolGateway.createSubscribeOptions = (platformProtocolOnly, adapterId) => new SubscribeOptions(platformProtocolOnly, adapterId);
ProtocolGateway.createPublishOptions = (platformProtocolOnly, adapterId) => new PublishOptions(platformProtocolOnly, adapterId);

module.exports = ProtocolGateway;