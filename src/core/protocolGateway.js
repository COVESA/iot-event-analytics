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
            const Class = module[adapterConfigModel.get('module.class')];

            const instance = new Class(adapterConfigModel.get('config'), displayName);

            this.adapters.push({
                instance,
                isPlatformProtocol,
                id: instance.getId()
            });
        }
    }

    async publish(topic, message, publishOptions = ProtocolGateway.createPublishOptions()) {
        let publishToPlatformProtocolOnly = publishOptions.platformProtocolOnly;

        if (publishToPlatformProtocolOnly === null) {
            // Apply global settings, if nothing is given
            publishToPlatformProtocolOnly = this.usePlatformProtocolOnly;
        }

        this.__validatePlatformProtocolUsage(publishToPlatformProtocolOnly);

        for (let adapter of this.adapters) {
            if (publishToPlatformProtocolOnly === false || adapter.isPlatformProtocol) {
                if (publishOptions.adapterId === null || publishOptions.adapterId === adapter.id) {
                    await adapter.instance.publish(topic, message, publishOptions);
                }
            }
        }
    }

    publishJson(topic, json, publishOptions) {
        return this.publish(topic, JSON.stringify(json), publishOptions);
    }

    // Callback needs to be (ev, topic) => {}
    async subscribe(topic, callback, subscribeOptions = ProtocolGateway.createSubscribeOptions()) {
        let subscribeToPlatformProtocolOnly = subscribeOptions.platformProtocolOnly;

        if (subscribeToPlatformProtocolOnly === null) {
            // Apply global settings, if nothing is given
            subscribeToPlatformProtocolOnly = this.usePlatformProtocolOnly;
        }

        this.__validatePlatformProtocolUsage(subscribeToPlatformProtocolOnly);

        for (let adapter of this.adapters) {
            if (subscribeToPlatformProtocolOnly === false || adapter.isPlatformProtocol) {
                if (subscribeOptions.adapterId === null || subscribeOptions.adapterId === adapter.id) {
                    await adapter.instance.subscribe(
                        topic,
                        ((adapter) => (ev, topic) => callback(ev, topic, adapter.id))(adapter), // Save adapter in IIFE
                        subscribeOptions
                    );
                }
            }
        }
    }

    // Callback needs to be (ev, topic) => {}
    subscribeJson(topic, callback, subscribeOptions) {
        return this.subscribe(topic, (stringifiedJson, topic, adapterId) => {
            try {
                callback(this.__tryParseJson(stringifiedJson), topic, adapterId);
            }
            catch(err) {
                // Could not parse Json from incoming message
            }
        }, subscribeOptions);
    }

    // Callback needs to be (ev, topic) => {}
    async subscribeShared(group, topic, callback, subscribeOptions = ProtocolGateway.createSubscribeOptions()) {
        let subscribeToPlatformProtocolOnly = subscribeOptions.platformProtocolOnly;

        if (subscribeToPlatformProtocolOnly === null) {
            // Apply global settings, if nothing is given
            subscribeToPlatformProtocolOnly = this.usePlatformProtocolOnly;
        }

        this.__validatePlatformProtocolUsage(subscribeToPlatformProtocolOnly);

        for (let adapter of this.adapters) {
            if (subscribeToPlatformProtocolOnly === false || adapter.isPlatformProtocol) {
                if (subscribeOptions.adapterId === null || subscribeOptions.adapterId === adapter.id) {

                    await adapter.instance.subscribeShared(
                        group,
                        topic,
                        ((adapter) => (ev, topic) => callback(ev, topic, adapter))(adapter), // Save adapter in IIFE
                        subscribeOptions
                    );
                }
            }
        }
    }

    // Callback needs to be (ev, topic) => {}
    subscribeJsonShared(group, topic, callback, subscribeOptions) {
        return this.subscribeShared(group, topic, (stringifiedJson, topic, adapterId) => {
            try {
                callback(this.__tryParseJson(stringifiedJson), topic, adapterId);
            }
            catch(err) {
                // Could not parse JSON from incoming message
            }
        }, subscribeOptions);
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