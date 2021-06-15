# IoTea Protocol Adapter Guide

## Introduction

The internal and external communication in IoT Event Analytics is abstracted using a provided Protocol Adapter. We are shipping IoT Event Analytics with an MQTT Protocol Adapter Implementation, but you can write your own adapter using a simple Interface and a program language specific Loading mechanism i.e. modules in NodeJS and packages in Python.

## Configuration

The configuration can be done programatically or explicitly by using a JSON file. In the end the programatic configuration creates a JSON Object, so it's just syntactic sugar and facilitates the configuration process by specifying oftenly used default values. An example configuration looks like this

```json
{
    "adapters": [                                       // All supported protocol adapters are configured in this list
        {
            "platform": true,                           // Property only evaluated in the IoT Event Analytics platform, since it specifies the internal communication protocol. Only one platform protocol allowed per configuration
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

The following programmatic generation resembles exactly the prior one. The only difference is, that the `brokerUrl` property is set to `mqtt://localhost:1883` per default.

```javascript
const adapterConfig = MqttProtocolAdapter.createDefaultConfiguration(true);
const gatewayConfig = ProtocolGateway.createDefaultConfiguration([ adapterConfig ]);
```

## How it works (JavaScript)

After instantiating a protocolGateway `const pg = new ProtocolGateway(gatewayConfig, 'my-display-name', <usePlatformProtocolOnly>);` it can be used to subscribe and publish messages over the underlying protocol, without knowing the protocol details. The interface is as follows:

```javascript
async publish(topic, message, ProtocolGateway.createPublishOptions(<platformProtocolOnly>, <adapterId>));
async subscribe(topic, callback, ProtocolGateway.createSubscribeOptions(<platformProtocolOnly>, <adapterId>));
async subscribeShared(group, topic, callback, ProtocolGateway.createSubscribeOptions(<platformProtocolOnly>, <adapterId>));
```

If you simply want to send a message over the gateway, call `pg.publish('some/topic', 'Hello World');`. This will publish the given message to all configured protocol adapters, if `<usePlatformProtocolOnly>` is set to false in the ProtocolGateway constructor. If this flag is set to true, the message will only be sent over the defined platform protocol adapter (`platform: true`).
If you created a protocol gateway instance with `<usePlatformProtocolOnly>` set to false, you can specify either the flag `<platformProtocolOnly>` in the PubSub options to restrict the message being sent or received only over the adapter having the `platform: true` flag set to true or you can specify the `adapterId`, which resembles the return value of the `getId()` function to send the message only via this adapter. If you set `<usePlatformProtocolOnly>` to true and `<platformProtocolOnly>` to false you will receive an error.

The protocol gateway will instantiate all configured protocol adapters in the background by using the given module or package information. It will pass the config section of the adapter configuration and the display name (from the ProtocolGateway constructor parameters) to the constructor.

The adapter has to offer the following interface:

```javascript
    abstract class MyAdapter {
        constructor(config, displayName);
        abstract publish(topic, message, publishOptions = {});
        abstract subscribe(topic, (message, topic) => {});
        abstract subscribeShared(group, topic, (message, topic) => {});
        abstract getId();
    }
```

You can implement your own adapter by following this interface. You can take the implementation of the MqttProtocolAdapter in _../../src/core/mqtt/mqttClient.js_ as an example.
