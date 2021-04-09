const {
    EventEmitter
} = require('events');

const mock = require('mock-require');
class MqttClientMock extends EventEmitter {
    constructor(url, supportsSharedSubscriptions = true) {
        super();
        this.url = url;
        this.connected = false;
        this.subscriptions = [];
        this.supportsSharedSubscriptions = supportsSharedSubscriptions;
    }

    mConnect() {
        // {} = Packet
        this.emit('connect', {});
        this.connected = true;
    }

    mClose() {
        this.emit('close', {});
        this.connected = false;
    }

    // eslint-disable-next-line no-unused-vars
    mPublish(topic, message, options, callback = (err, packet) => {}) {
        for (let subscriptionRegex of this.subscriptions) {
            if (subscriptionRegex.test(topic)) {
                // Only do this, if there is a subscription for the given topic
                this.emit('message', topic, Buffer.from(message, 'utf8'));
                break;
            }

            subscriptionRegex.lastIndex = 0;
        }

        setImmediate(() => {
            // {} = Packet
            callback(undefined, {});
        });
    }

    // eslint-disable-next-line no-unused-vars
    mOnSubscription(topic, options, callback = (err, granted) => {}) {
        // If topic starts with  >> remove it
        callback(undefined, {});
    }

    // eslint-disable-next-line no-unused-vars
    mOnUnsubscribe(topics, options, callback = (err, packet) => {}) {
        // {} = Packet
        callback(undefined, {});
    }

    subscribe(topic, options, callback) {
        const regex = this.__createRegexForTopic(topic);

        let addedSubscriptionRegex = false;

        try {
            if (this.subscriptions.findIndex(subscriptionRegex => subscriptionRegex.source === regex.source) === -1) {
                this.subscriptions.push(regex);
                addedSubscriptionRegex = true;
            }

            this.mOnSubscription(topic, options, callback);
        } catch(err) {
            if (addedSubscriptionRegex) {
                this.subscriptions.splice(0, -1);
            }

            throw err;
        }
    }

    unsubscribe(topics, options, callback) {
        for (let topic of topics) {
            const regex = this.__createRegexForTopic(topic);
            this.subscriptions = this.subscriptions.filter(subscriptionRegex => subscriptionRegex.source !== regex.source);
        }

        this.mOnUnsubscribe(topics, options, callback);
    }

    publish(topic, message, options, callback) {
        this.mPublish(topic, message, options, callback);
    }

    // eslint-disable-next-line no-unused-vars
    end(force, options, callback = () => {}) {
        // Simply close mock - not supported
        this.mClose();
        callback();
    }

    __createRegexForTopic(topic) {
        if (this.supportsSharedSubscriptions) {
            // Remove $share/<groupId>/
            // eslint-disable-next-line no-useless-escape
            topic = topic.replace(/^\$share\/[^\/]+\//, '');
        }

        // Add regex to subscriptions
        return new RegExp(`^${topic.replace(/\$/g, '\\$').replace(/\//g, '\\/').replace(/\+/g, '[^\\/]+').replace(/#/, '.+')}$`, 'g');
    }
}

function prepareMockedMqttClient(clientMock = new MqttClientMock()) {
    mock.stop('mqtt');

    mock('mqtt', {
        connect: () => clientMock
    });

    return clientMock;
}

module.exports = {
    prepareMockedMqttClient,
    MqttClientMock
};