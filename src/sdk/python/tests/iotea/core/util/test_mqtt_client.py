import asyncio
import json
import uuid
from unittest.mock import ANY
from unittest import TestCase

import pytest
from hbmqtt.session import IncomingApplicationMessage
from hbmqtt.mqtt.constants import QOS_0

from src.iotea.core.util.mqtt_client import MqttClient

MOCK_PROBE = '30303030'
delivered = False
callback_invoked = False


@pytest.fixture
def test_case():
    return TestCase()

# pylint: disable=unused-argument
async def mock_connect(broker_url, cleansession=True):
    pass

# pylint: disable=unused-argument
async def mock_subscribe(topics):
    pass

# pylint: disable=unused-argument
async def mock_unsubscribe(topics):
    pass

async def mock_publish(topic, message, qos=None, retain=None, ack_timeout=None):
    pass

async def mock_reconnect(cleansession=True):
    pass

async def mock_disconnect(cleansession=True):
    pass

@pytest.fixture
def mock_deliver(topic_ns, topic, message):
    async def mock_deliver_message():
        # pylint: disable=global-statement
        global delivered
        in_message = None
        if delivered:
            await asyncio.sleep(0.2)
        else:
            delivered = True
            message_topic = topic if topic_ns is None else f'{topic_ns}{topic}'
            in_message = IncomingApplicationMessage('packet_id', message_topic, QOS_0, message.encode('utf-8'),
                                                    False)
            in_message.publish_packet = in_message.build_publish_packet()
        return in_message

    return mock_deliver_message


def mock_uuid():
    # resulting mock is 30303030
    return uuid.UUID(bytes=b'0000000000000000', version=4)


# topic_ns, topic and message parameters are passed to mock_deliver
@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', [f'probe/{MOCK_PROBE}'])
@pytest.mark.parametrize('message', [f'probe-{MOCK_PROBE}'])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
# pylint: disable=too-many-arguments
async def test_check_mqtt5(mocker, mock_deliver, topic_ns):
    # pylint: disable=global-statement
    global delivered
    delivered = False

    # pylint: disable=unused-argument
    async def mock_publish_probe(topic, message, qos=None, retain=None, ack_timeout=None):
        # give time to mqtt_client.__run_on_message to startup
        # first subscription is mqtt5 probe
        # actual publish takes some time for i/o operations
        await asyncio.sleep(0.2)
        probe_tag = 'probe/'
        probe_tag_index = topic.find(probe_tag)
        if probe_tag_index > -1:
            probe = topic[probe_tag_index + len(probe_tag):]
            assert probe == MOCK_PROBE, 'Probe in publish is not the same as generated mock probe!'
            await mqtt_client.client.deliver_message()

    # uuid4 is imported in mqtt_client and can be found only as a function of mqtt_client.uuid4 and not as uuid.uuid4
    mocker.patch('src.iotea.core.util.mqtt_client.uuid4', wraps=mock_uuid)
    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.subscribe', wraps=mock_subscribe)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish_probe)
    mocker.patch('hbmqtt.client.MQTTClient.unsubscribe', wraps=mock_unsubscribe)
    mocker.patch('hbmqtt.client.MQTTClient.deliver_message', wraps=mock_deliver)

    test_topic = 'test_topic'
    test_message = 'test message'

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=True)
    await mqtt_client.publish([test_topic], test_message)
    assert mqtt_client.client.connect.call_count == 1
    assert mqtt_client.client.publish.call_count == 2
    mqtt_client.client.publish.assert_called_with(f'{topic_ns}{test_topic}', test_message.encode('utf-8'), qos=0,
                                                  retain=False)


@pytest.mark.parametrize('topic_ns', ['namespace/', None])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['{"test": "message"}'])
@pytest.mark.parametrize('to_json', [False, True])
@pytest.mark.asyncio
# test the common scenario: subscribe (mqtt_client.subscribe, mqtt_client.subscribe_json) for a topic,
# publish (mqtt_client.publish, mqtt_client.publish_json) to the topic
# and check if subscription callbacks are properly invoked
# pylint: disable=redefined-outer-name, too-many-arguments, too-many-locals
async def test_pub_sub(test_case, mocker, mock_deliver, topic_ns, topic, message, to_json):
    # pylint: disable=global-statement
    global callback_invoked, delivered
    callback_invoked = False
    delivered = False

    async def mock_publish_with_deliver(topic, message, qos=None, retain=None, ack_timeout=None):
        # give time to mqtt_client.__run_on_message to start
        await asyncio.sleep(0.2)
        await mqtt_client.client.deliver_message()

    def sync_callback(c_message, c_topic):
        # pylint: disable=global-statement
        global callback_invoked
        callback_invoked = True
        expected_msg = message
        if to_json:
            expected_msg = json.loads(message)
        test_case.assertEqual(expected_msg, c_message)
        test_case.assertEqual(expected_topic, c_topic)

    async def async_callback(c_message, c_topic):
        sync_callback(c_message, c_topic)

    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.subscribe', wraps=mock_subscribe)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish_with_deliver)
    mocker.patch('hbmqtt.client.MQTTClient.unsubscribe', wraps=mock_unsubscribe)
    mocker.patch('hbmqtt.client.MQTTClient.deliver_message', wraps=mock_deliver)

    callbacks = [sync_callback, async_callback]
    i = 0
    for callback in callbacks:
        i += 1
        mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)

        if to_json:
            await mqtt_client.subscribe_json(topic, callback)
        else:
            await mqtt_client.subscribe(topic, callback, to_json)

        assert mqtt_client.client.connect.call_count == i
        assert mqtt_client.client.subscribe.call_count == i
        expected_topic = topic if topic_ns is None else f'{topic_ns}{topic}'
        # assert if topic namespace is prepended
        mqtt_client.client.subscribe.assert_called_with([(expected_topic, QOS_0)])
        if to_json:
            await mqtt_client.publish_json(topic, json.loads(message))
        else:
            await mqtt_client.publish(topic, message)
        # connect should not be called again
        assert mqtt_client.client.connect.call_count == i
        assert mqtt_client.client.publish.call_count == i

        mqtt_client.client.publish.assert_called_with(expected_topic, ANY, qos=0,
                                                      retain=False)
        # pylint: disable=unused-variable
        args, kwargs = mqtt_client.client.publish.call_args
        json_rcvd_message = args[1]
        if isinstance(json_rcvd_message, (str, bytes)):
            json_rcvd_message = json.loads(json_rcvd_message)
        json_sent_message = json.loads(message)
        test_case.assertEqual(json_sent_message, json_rcvd_message)
        # give some time to deliver the message to the callbacks
        await asyncio.sleep(0.1)
        assert callback_invoked is True, "Subscribe callback was not invoked!"

        await mqtt_client.unsubscribe(topic)
        assert mqtt_client.client.unsubscribe.call_count == i
        mqtt_client.client.unsubscribe.assert_called_with([expected_topic])


@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['{"test": "message"}'])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_pub_no_stash(test_case, mocker, topic_ns, topic, message):
    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish)

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    mqtt_client.client.disconnected = True
    await mqtt_client.publish('topic', 'message', options=None, stash=False)

    assert mqtt_client.client.connect.call_count == 0
    assert mqtt_client.client.publish.call_count == 0


@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['{"test": "message"}'])
@pytest.mark.asyncio
#send a publish message, disconnect and reconnect, send another publish message
#check if the client functions after disconnect and reconnect
# pylint: disable=redefined-outer-name
async def test_reconnect(test_case, mocker, topic_ns, topic, message):
    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish)
    mocker.patch('hbmqtt.client.MQTTClient.reconnect', wraps=mock_reconnect)
    mocker.patch('hbmqtt.client.MQTTClient.disconnect', wraps=mock_disconnect)

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)

    await mqtt_client.publish(topic, message, options=None)

    assert mqtt_client.client.connect.call_count == 1
    assert mqtt_client.client.publish.call_count == 1
    mqtt_client.client.publish.assert_called_once_with(f'{topic_ns}{topic}', message.encode('utf-8'), qos=QOS_0, retain=False)

    await mqtt_client.disconnect()
    assert mqtt_client.client.disconnect.call_count == 1

    await mqtt_client.client.reconnect(cleansession=True)

    assert mqtt_client.client.disconnected is False

    message = 'new message'
    await mqtt_client.publish(topic, message, options=None)

    assert mqtt_client.client.connect.call_count == 1
    assert mqtt_client.client.publish.call_count == 2
    mqtt_client.client.publish.assert_called_with(f'{topic_ns}{topic}', message.encode('utf-8'), qos=QOS_0, retain=False)

# pylint: disable=redefined-outer-name
def test_wrong_namespace(test_case):
    topic_ns = 'no_trailing_slash'
    with pytest.raises(Exception) as exc_info:
        MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    assert f'Given topic namespace {topic_ns} is invalid. It has to have a trailing slash' == str(exc_info.value)


@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['invalid_json_message'])
@pytest.mark.asyncio
async def test_validate_json(test_case, topic_ns, topic, message):
    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    with pytest.raises(Exception) as exc_info:
        await mqtt_client.publish_json(topic, message)
    assert f'Given JSON document is neither a dictionary nor a list' == str(exc_info.value)


@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['{"test": "message"}'])
@pytest.mark.asyncio
async def test_multiple_connects(test_case, mocker, topic_ns, topic, message):
    connect_timeout = 2
    async def mock_connect_block(broker_url, cleansession=True):
        #slow down the connect method to be able to trigger parallel connect invocations from publish
        await asyncio.sleep(connect_timeout/2)

    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect_block)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish)

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)

    asyncio.get_event_loop().create_task(mqtt_client.publish(topic, message))
    #wait a bit to start the publish task
    await asyncio.sleep(connect_timeout/3)

    mqtt_client.client.connect.assert_called_once()
    assert mqtt_client.client.publish.call_count == 0
    assert mqtt_client.connecting is True
    #send another publish request, while still connecting
    await mqtt_client.publish(topic, message)
    assert mqtt_client.connecting is False

    #assert connect is called only once event though parallel publish methods lead to connect
    mqtt_client.client.connect.assert_called_once()
    assert mqtt_client.client.publish.call_count == 2


@pytest.mark.parametrize('topic_ns', ['namespace/', None])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['bad json message'])
@pytest.mark.asyncio
# pylint: disable=too-many-arguments
async def test_delivered_bad_message(test_case, mocker, mock_deliver, topic_ns, topic, message):
    # pylint: disable=global-statement
    global callback_invoked, delivered
    callback_invoked = False
    delivered = False

    async def mock_publish(topic, message, qos=None, retain=None, ack_timeout=None):
        # give time to mqtt_client.__run_on_message to start
        await asyncio.sleep(0.2)
        await mqtt_client.client.deliver_message()

    def sync_callback(c_message, c_topic):
        # pylint: disable=global-statement
        global callback_invoked
        callback_invoked = True
        test_case.assertEqual(json.loads(message), c_message)
        test_case.assertEqual(f'{topic_ns}{topic}', c_topic)


    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.subscribe', wraps=mock_subscribe)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish)
    mocker.patch('hbmqtt.client.MQTTClient.unsubscribe', wraps=mock_unsubscribe)
    mocker.patch('hbmqtt.client.MQTTClient.deliver_message', wraps=mock_deliver)

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    await mqtt_client.subscribe_json(topic, sync_callback)

    assert mqtt_client.client.connect.call_count == 1
    assert mqtt_client.client.subscribe.call_count == 1
    expected_topic = topic if topic_ns is None else f'{topic_ns}{topic}'
    # assert if topic namespace is prepended
    mqtt_client.client.subscribe.assert_called_with([(expected_topic, QOS_0)])

    await mqtt_client.publish(topic, message)

    assert mqtt_client.client.publish.call_count == 1

    # give some time to deliver the message to the callbacks
    await asyncio.sleep(0.1)
    assert callback_invoked is False, "Subscribe callback was invoked while the message can not be validated!"

    await mqtt_client.unsubscribe(topic)
    assert mqtt_client.client.unsubscribe.call_count == 1
