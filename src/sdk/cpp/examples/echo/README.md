# IoTea C++ SDK - Echo Example

The echo example consists of multiple talents:


## echo_provider

This is the central talent of this example. It offers an echo "service" consisting of these elements:

**Function feature `echo_provider.echo`:**

Receives a string-based message, converts it to uppercase (in sense of 'std::toupper'), and returns the converted uppercase message as an 'echo' to the caller

**Event feature `echo_provider.echoCount`:**

Each time the echo function gets called, it increases an internal counter and publishes the updated counter via this event.

**Event feature `echo_provider.echoResponseSent`:**

Each time the echo function gets called, the uppercase converted message is published via this event.


All features are dynamic, i.e. of default type and part of the default segment (`"000000"`).


## echo_consumer

Calls the `echo` function of the `echo_provider` with sending a string-based message and receives the reply.

The message string must be injected publishing this message to the MQTT broker:

    { "subject": "someuserid", "type": "default", "instance": "4711", "feature": "echo_consumer.messageString", "value": "Hello", "whenMs": 1626566400000 }


## echo_observer

Subscribes the events offered by the `echo_provider` and receives and logs respective notifications.

