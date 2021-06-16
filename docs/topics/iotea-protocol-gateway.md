# IoT Event Analytics Protocol Gateway

## Introduction

The internal and external communication in IoT Event Analytics is abstracted using the Protocol Gateway. The Gateway itself manages and accesses underlying Protocol Adapters, which themselves work on a specific communication protocol. IoT Event Analytics is shipped with an [MQTT Protocol Adapter Implementation](../../src/core/util/mqttClient.js).

## Configuration of the Protocol Gateway

The configuration can be done programatically or explicitly by using a JSON file. In the end the programatic configuration creates a JSON Object, so it's just syntactic sugar and facilitates the configuration process by specifying oftenly used default values. An example configuration for an instance of the Protocol Gateway looks like this:

```json
{
    "adapters": [                                       // All supported protocol adapters are configured in this list
        {
            "platform": true,                           // Property only evaluated in the IoT Event Analytics platform, since it specifies the communication protocol used internally. Only one protocol adapter is allowed to be flagged as being the platform protocol
            "module": {                                 // Module / Package configuration i.e. where to load what. Class is optional, if the file has a default or non-named export in NodeJS. Note, that in Python the package delimiter is a dot i.e. the package path, in node it's the actual module path in the file system.
                "name": "./util/mqttClient",
                "class": "MqttProtocolAdapter"
            },
            "config": {                                 // The adapter configuration, if needed. This holds the adapter specific configuration
                "topicNamespace": "iotea/",
                "brokerUrl": "mqtt://mosquitto:1883"
            }
        }
    ]
}
```

The ProtocolGateway will instantiate all configured protocol adapters in the background by using the given module or package information. It will pass the config section of the adapter configuration and the display name (from the ProtocolGateway constructor parameters) to the constructor ot the Protocol Adapter.

The following programmatic generation resembles exactly the prior one. The only difference is, that the `brokerUrl` property is set to `mqtt://localhost:1883` per default.

```javascript
const adapterConfig = MqttProtocolAdapter.createDefaultConfiguration(true);
const gatewayConfig = ProtocolGateway.createDefaultConfiguration([ adapterConfig ]);
```

## How it works (JavaScript)

After instantiating a protocolGateway `const pg = new ProtocolGateway(gatewayConfig, 'my-display-name', <usePlatformProtocolOnly>);` it can be used to subscribe and publish messages over the underlying protocol(s), without knowing the protocol details. The interface is as follows:

- `<usePlatformProtocolOnly>`: Global configuration to restrict all pub/sub operations to only use the platform protocol adapter (See `platform: true` in the above Protocol Gateway configuration example).

```javascript
async publish(topic, message, ProtocolGateway.createPublishOptions(<platformProtocolOnly>, <adapterId>));
async subscribe(topic, callback, ProtocolGateway.createSubscribeOptions(<platformProtocolOnly>, <adapterId>));
async subscribeShared(group, topic, callback, ProtocolGateway.createSubscribeOptions(<platformProtocolOnly>, <adapterId>));
```

- `<platformProtocolOnly> optional`: Override the global `<usePlatformProtocolOnly>` setting. Only works, if `<usePlatformProtocolOnly> == false`, since if `<usePlatformProtocolOnly> == true` the platform protocol adapter is the only one instantiated and the others are skipped completly. Otherwise you will get an error.
- `<adapterId> optional`: The unique value, which the `ProtocolAdapter.getId()` returns, to identify a specific adapter instance. If set, you can execute your pub/sub operation solely on the adapter with the given `<adapterId>`. If `<usePlatformProtocolOnly> == true` and the given `<adapterId>` differs from the Id of the platform protocol adapter, you get an error.

```text
|--------------------------------------------------------------------------------------------------------|
|                                 | <usePlatformProtocolOnly> = true | <usePlatformProtocolOnly> = false |
| <platformProtocolOnly> = true   |                OK                |                OK                 |
| <platformProtocolOnly> = false  |               ERROR              |                OK                 |
| <adapterId> = "non-platform-id" |               ERROR              |                OK                 |
| <adapterId> = "platform-id"     |                OK                |                OK                 |
|--------------------------------------------------------------------------------------------------------|
```

The PublishOption class has two additional properties, which are not set via the mentioned factory functions.

- `retain (default: false)`: Saves the last message published to a given topic. If a new subscriber subscribes to this topic, it will imediatelly receive this retained message. Only one message can be retained per topic. It'll be replaced by the next message, which will be retained for the same topic.
- `stash (default: true)`: If a message cannot be sent due to connection problems and stashing is switched on, the message will be buffered until the connection is reestablished again. Otherwise it'll be discarded

If you want to send a message over the gateway and completly want to use all the defaults, simply call `pg.publish('some/topic', 'Hello World');`

### The Protocol Adapter Interface

```javascript
    abstract class MyAdapter{
        constructor(config, displayName);
        abstract publish(topic, message, publishOptions = {});
        abstract subscribe(topic, (message, topic) => {});
        abstract subscribeShared(group, topic, (message, topic) => {});
        abstract getId();
    }
```

#### subscribeShared(`<group>`, `<topic>`, `(message, topic) => {}`)

If you use this method to subscribe to a given `<topic>`, your subscription will be put in the topic specific `<group>`. All subscribers in the same `<group>`, will receive incoming messages in a round-robin like fashion. This method allows automatic load balancing, which is handled by the underlying protocol adapter i.e. Each message is received by a single subscriber within the group.

You can implement your own adapter by following this interface. You can take the implementation of the [MqttProtocolAdapter](../../src/core/util/mqttClient.js) as an example.
