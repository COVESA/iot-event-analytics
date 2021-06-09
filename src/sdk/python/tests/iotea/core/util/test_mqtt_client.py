import asyncio
import contextlib
import json
import uuid
from unittest.mock import ANY
from unittest import TestCase

import pytest
from hbmqtt.session import IncomingApplicationMessage
from hbmqtt.mqtt.constants import QOS_0

from src.iotea.core.util.mqtt_client import MqttClient

# timeout for blocking operations on asyncio synchronization primitives
TIMEOUT = 10
MOCK_PROBE = '30303030'

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

#deliver_controls is a list [max_deliver_invocations, current_count_of_deliver_invocations]
def mock_deliver(deliver_event, deliver_controls, topic_ns, topic, message):
    async def mock_deliver_message():
        if deliver_event.is_set():
            # simulate some i/o blocking, otherwise the deliver method is called nons-top by __run_on_message loop
            # and takes all cpu, not giving a chance to other tasks
            await asyncio.sleep(0.2)
        else:
            deliver_event.set()
        in_message = None
        #the very first deliver_message invocation is only used for verification that __run_on_message is running,
        # so in_message=None in this case
        if deliver_controls[1] < deliver_controls[0] and deliver_controls[0] > 0:
            message_topic = topic if topic_ns is None else f'{topic_ns}{topic}'
            in_message = IncomingApplicationMessage('packet_id', message_topic, QOS_0, message.encode('utf-8'),
                                                    False)
            in_message.publish_packet = in_message.build_publish_packet()
        deliver_controls[1] += 1
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
async def test_check_mqtt5(test_case, mocker, topic_ns, topic, message):
    deliver_event = asyncio.Event()
    #2 deliver_message calls are expected: one to get __run_on_message and one for the probe message
    deliver_controls = [2, 0]

    # pylint: disable=unused-argument
    async def mock_publish_probe(topic, message, qos=None, retain=None, ack_timeout=None):
        # give time to mqtt_client.__run_on_message to startup
        # deliver_message is called by mqtt_client.__run_on_message, this is our indication that __run_on_message loop is running
        await asyncio.wait_for(deliver_event.wait(), TIMEOUT)
        test_case.assertTrue(deliver_event.is_set(), 'deliver_message method not invoked! __run_on_message loop is not running!')
        probe_tag = 'probe/'
        probe_tag_index = topic.find(probe_tag)
        if probe_tag_index > -1:
            probe = topic[probe_tag_index + len(probe_tag):]
            test_case.assertEqual(MOCK_PROBE, probe, 'Probe in publish is not the same as generated mock probe!')
            await mqtt_client.client.deliver_message()

    # uuid4 is imported in mqtt_client and can be found only as a function of mqtt_client.uuid4 and not as uuid.uuid4
    mocker.patch('src.iotea.core.util.mqtt_client.uuid4', wraps=mock_uuid)
    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.subscribe', wraps=mock_subscribe)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish_probe)
    mocker.patch('hbmqtt.client.MQTTClient.unsubscribe', wraps=mock_unsubscribe)
    mocker.patch('hbmqtt.client.MQTTClient.deliver_message',
                 wraps=mock_deliver(deliver_event, deliver_controls, topic_ns, topic, message))

    test_topic = 'test_topic'
    test_message = 'test message'

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=True)
    await mqtt_client.publish(test_topic, test_message)
    test_case.assertEqual(1, mqtt_client.client.connect.call_count)
    test_case.assertEqual(2, mqtt_client.client.publish.call_count)
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
async def test_pub_sub(test_case, mocker, topic_ns, topic, message, to_json):
    # pylint: disable=global-statement
    callback_invoked_event = asyncio.Event()
    deliver_event = asyncio.Event()
    # 2 deliver_message calls are expected: one to get __run_on_message and one for the actual publish message
    deliver_controls = [2, 0]

    async def mock_publish_with_deliver(topic, message, qos=None, retain=None, ack_timeout=None):
        # give time to mqtt_client.__run_on_message to start
        # deliver_message is called by mqtt_client.__run_on_message and that will be indication that __run_on_message is running
        await asyncio.wait_for(deliver_event.wait(), TIMEOUT)
        test_case.assertTrue(deliver_event.is_set(), 'deliver_message method not invoked! __run_on_message loop is not running!')
        await mqtt_client.client.deliver_message()

    def sync_callback(c_message, c_topic):
        # pylint: disable=global-statement
        callback_invoked_event.set()
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
    mocker.patch('hbmqtt.client.MQTTClient.deliver_message',
                 wraps=mock_deliver(deliver_event, deliver_controls, topic_ns, topic, message))

    callbacks = [sync_callback, async_callback]
    for i, callback in enumerate(callbacks, start=1):
        mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)

        if to_json:
            await mqtt_client.subscribe_json(topic, callback)
        else:
            await mqtt_client.subscribe(topic, callback, to_json)

        test_case.assertEqual(i, mqtt_client.client.connect.call_count)
        test_case.assertEqual(i, mqtt_client.client.subscribe.call_count)
        expected_topic = topic if topic_ns is None else f'{topic_ns}{topic}'
        # assert if topic namespace is prepended
        mqtt_client.client.subscribe.assert_called_with([(expected_topic, QOS_0)])
        if to_json:
            await mqtt_client.publish_json(topic, json.loads(message))
        else:
            await mqtt_client.publish(topic, message)

        # connect should not be called again
        test_case.assertEqual(i, mqtt_client.client.connect.call_count)
        test_case.assertEqual(i, mqtt_client.client.publish.call_count)

        mqtt_client.client.publish.assert_called_with(expected_topic, ANY, qos=0,
                                                      retain=False)
        args, _ = mqtt_client.client.publish.call_args
        json_rcvd_message = args[1]
        if isinstance(json_rcvd_message, (str, bytes)):
            json_rcvd_message = json.loads(json_rcvd_message)
        json_sent_message = json.loads(message)
        test_case.assertEqual(json_sent_message, json_rcvd_message)

        await asyncio.wait_for(callback_invoked_event.wait(), TIMEOUT)
        test_case.assertTrue(callback_invoked_event.is_set(), "Subscribe callback was not invoked!")
        callback_invoked_event.clear()

        await mqtt_client.unsubscribe(topic)
        test_case.assertEqual(i, mqtt_client.client.unsubscribe.call_count)
        mqtt_client.client.unsubscribe.assert_called_with([expected_topic])
        deliver_controls[1] = 0

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

    test_case.assertEqual(0, mqtt_client.client.connect.call_count)
    test_case.assertEqual(0, mqtt_client.client.publish.call_count)


@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['{"test": "message"}'])
@pytest.mark.asyncio
# send a publish message, disconnect and reconnect, send another publish message
# check if the client functions after disconnect and reconnect
# pylint: disable=redefined-outer-name
async def test_reconnect(test_case, mocker, topic_ns, topic, message):
    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish)
    mocker.patch('hbmqtt.client.MQTTClient.reconnect', wraps=mock_reconnect)
    mocker.patch('hbmqtt.client.MQTTClient.disconnect', wraps=mock_disconnect)

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)

    await mqtt_client.publish(topic, message, options=None)

    test_case.assertEqual(1, mqtt_client.client.connect.call_count)
    test_case.assertEqual(1, mqtt_client.client.publish.call_count)
    mqtt_client.client.publish.assert_called_once_with(f'{topic_ns}{topic}', message.encode('utf-8'), qos=QOS_0,
                                                       retain=False)

    await mqtt_client.disconnect()
    test_case.assertEqual(1, mqtt_client.client.disconnect.call_count)

    await mqtt_client.client.reconnect(cleansession=True)

    test_case.assertFalse(mqtt_client.client.disconnected)

    message = 'new message'
    await mqtt_client.publish(topic, message, options=None)

    test_case.assertEqual(1, mqtt_client.client.connect.call_count)
    test_case.assertEqual(2, mqtt_client.client.publish.call_count)
    mqtt_client.client.publish.assert_called_with(f'{topic_ns}{topic}', message.encode('utf-8'), qos=QOS_0,
                                                  retain=False)


# pylint: disable=redefined-outer-name
def test_wrong_namespace(test_case):
    topic_ns = 'no_trailing_slash'
    with pytest.raises(Exception) as exc_info:
        MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    test_case.assertEqual(f'Given topic namespace {topic_ns} is invalid. It has to have a trailing slash', str(exc_info.value))


@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['invalid_json_message'])
@pytest.mark.asyncio
async def test_validate_json(test_case, topic_ns, topic, message):
    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    with pytest.raises(Exception) as exc_info:
        await mqtt_client.publish_json(topic, message)
    test_case.assertEqual(f'Given JSON document is neither a dictionary nor a list', str(exc_info.value))


@pytest.mark.parametrize('topic_ns', ['namespace/'])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['{"test": "message"}'])
@pytest.mark.asyncio
async def test_multiple_connects(test_case, mocker, topic_ns, topic, message):
    def mock_publish(publish_semaphore):
        async def f(topic, message, qos=None, retain=None, ack_timeout=None):
            publish_semaphore.release()

        return f

    def mock_connect_block(connected_event_e, blocking_event_e):
        async def f(broker_url, cleansession=True):
            connected_event_e.set()
            # slow down the connect method to be able to trigger parallel connect invocations from publish
            await asyncio.wait_for(blocking_event_e.wait(), TIMEOUT)

        return f

    publish_semaphore = asyncio.Semaphore(0)
    blocking_event = asyncio.Event()
    connected_event = asyncio.Event()

    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect_block(connected_event, blocking_event))
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish(publish_semaphore))

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    # create first publish task, it will trigger the connect method
    asyncio.get_event_loop().create_task(mqtt_client.publish(topic, message))
    await asyncio.wait_for(connected_event.wait(), TIMEOUT)
    mqtt_client.client.connect.assert_called_once()
    test_case.assertEqual(0, mqtt_client.client.publish.call_count)
    test_case.assertFalse(mqtt_client.client_initialized)

    # send another publish request, while still connecting
    asyncio.get_event_loop().create_task(mqtt_client.publish(topic, message))

    # unblock the connect method
    blocking_event.set()

    # wait for 2 publish invocations
    await asyncio.wait_for(publish_semaphore.acquire(), TIMEOUT)
    await asyncio.wait_for(publish_semaphore.acquire(), TIMEOUT)

    test_case.assertEqual(2, mqtt_client.client.publish.call_count)

    # assert connect is called only once even though parallel publish methods lead to connect
    mqtt_client.client.connect.assert_called_once()
    test_case.assertTrue(mqtt_client.client_initialized)


@pytest.mark.parametrize('topic_ns', ['namespace/', None])
@pytest.mark.parametrize('topic', ['test_topic'])
@pytest.mark.parametrize('message', ['bad json message'])
@pytest.mark.asyncio
# pylint: disable=too-many-arguments
async def test_delivered_bad_message(test_case, mocker, topic_ns, topic, message):
    # pylint: disable=global-statement
    callback_invoked_event = asyncio.Event()
    deliver_event = asyncio.Event()
    #deliver_controls: max_count, current_count
    # 2 deliver_message calls are expected: one to get __run_on_message and one for the probe message
    deliver_controls = [2, 0]

    async def mock_publish(topic, message, qos=None, retain=None, ack_timeout=None):
        # give time to mqtt_client.__run_on_message to start
        # deliver_message is the indication that __run_on_message task is running
        await asyncio.wait_for(deliver_event.wait(), TIMEOUT)
        test_case.assertTrue(deliver_event.is_set(), 'deliver_message method not invoked! __run_on_message loop is not running!')
        await mqtt_client.client.deliver_message()

    def sync_callback(c_message, c_topic):
        # pylint: disable=global-statement
        callback_invoked_event.set()
        test_case.assertEqual(json.loads(message), c_message)
        test_case.assertEqual(f'{topic_ns}{topic}', c_topic)

    mocker.patch('hbmqtt.client.MQTTClient.connect', wraps=mock_connect)
    mocker.patch('hbmqtt.client.MQTTClient.subscribe', wraps=mock_subscribe)
    mocker.patch('hbmqtt.client.MQTTClient.publish', wraps=mock_publish)
    mocker.patch('hbmqtt.client.MQTTClient.unsubscribe', wraps=mock_unsubscribe)
    mocker.patch('hbmqtt.client.MQTTClient.deliver_message',
                 wraps=mock_deliver(deliver_event, deliver_controls, topic_ns, topic, message))

    mqtt_client = MqttClient('mqtt://localhost:1883', topic_ns, check_mqtt5_compatibility=False)
    await mqtt_client.subscribe_json(topic, sync_callback)

    test_case.assertEqual(1, mqtt_client.client.connect.call_count)
    test_case.assertEqual(1, mqtt_client.client.subscribe.call_count)
    expected_topic = topic if topic_ns is None else f'{topic_ns}{topic}'
    # assert if topic namespace is prepended
    mqtt_client.client.subscribe.assert_called_with([(expected_topic, QOS_0)])

    await mqtt_client.publish(topic, message)
    test_case.assertEqual(1, mqtt_client.client.publish.call_count)

    # give some time to deliver the message to the callbacks
    # smaller timeout, as the event is not expected to be set
    with contextlib.suppress(asyncio.TimeoutError):
        await asyncio.wait_for(callback_invoked_event.wait(), 1)

    test_case.assertFalse(callback_invoked_event.is_set(), "Subscribe callback was invoked while the message can not be validated!")

    await mqtt_client.unsubscribe(topic)
    test_case.assertEqual(1, mqtt_client.client.unsubscribe.call_count)
