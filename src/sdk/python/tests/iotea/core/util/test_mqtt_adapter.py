import asyncio
import unittest.mock
from unittest import TestCase

import pytest
from src.iotea.core.util.mqtt_client import MqttProtocolAdapter
from src.iotea.core.protocol_gateway import ProtocolGateway


@pytest.fixture
def test_case():
    return TestCase()


@pytest.fixture
def mqtt_adapter(name, topic_namespace):
    if topic_namespace is None:
        mqtt_config = {"platform": False,
                       "module": {"name": ".util.mqtt_client", "class": "MqttProtocolAdapter"},
                       "config": {"brokerUrl": "mqtt://localhost:1883"}}
    else:
        mqtt_config = {"platform": False,
                       "module": {"name": ".util.mqtt_client", "class": "MqttProtocolAdapter"},
                       "config": {"brokerUrl": "mqtt://localhost:1883", "topicNamespace": topic_namespace}}

    return MqttProtocolAdapter(mqtt_config['config'], name)


def prepend_namespace(topic_namespace, topic):
    if topic_namespace is None:
        return topic
    return f'{topic_namespace}{topic}'


@pytest.mark.parametrize("topic", ['test', 'iotea'])
@pytest.mark.parametrize("name", [None, 'MqttAdapterDisplayName'])
@pytest.mark.parametrize("topic_namespace", ['iotea/'])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_subscribe(mocker, mqtt_adapter, topic_namespace, topic):
    # pylint: disable=unused-argument
    async def mock_subscribe(topic, callback, to_json=False):
        pass

    mocker.patch('src.iotea.core.util.mqtt_client.MqttClient.subscribe', wraps=mock_subscribe)

    await mqtt_adapter.subscribe(topic, lambda: {}, None)
    # check if topicNs is prepended if not already present
    mqtt_adapter.client.subscribe.assert_called_once_with(prepend_namespace(topic_namespace, topic), unittest.mock.ANY)


def __strip_topic_namespace(topic, namespace):
    if namespace is None:
        return topic
    topic_ns_index = topic.find(namespace)
    if topic_ns_index != 0:
        return topic
    return topic[len(namespace):]


@pytest.mark.parametrize("name", [None, 'MqttAdapterDisplayName'])
@pytest.mark.parametrize("topic_namespace", ['iotea/', None])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_subscribe_callback(test_case, mocker, mqtt_adapter, topic_namespace):
    test_topic = 'test'
    test_message = 'test message'

    async def mock_async_callback(message, topic):
        mock_sync_callback(message, topic)

    def mock_sync_callback(message, topic):
        test_case.assertEqual(test_message, message)
        # the callback must be called with the topic with stripped_off topic namespace
        test_case.assertEqual(test_topic, topic)

    # pylint: disable=unused-argument
    async def mock_subscribe(topic, callback, to_json=False):
        if asyncio.iscoroutinefunction(callback):
            await callback(test_message, topic)
        else:
            callback(test_message, topic)

    mocker.patch('src.iotea.core.util.mqtt_client.MqttClient.subscribe', wraps=mock_subscribe)

    callbacks = [mock_async_callback, mock_sync_callback]
    for callback in callbacks:
        await mqtt_adapter.subscribe(test_topic, callback, None)
        mqtt_adapter.client.subscribe.assert_called_with(prepend_namespace(topic_namespace, test_topic), unittest.mock.ANY)


@pytest.mark.parametrize("name", [None, 'MqttAdapterDisplayName'])
@pytest.mark.parametrize("topic_namespace", ['iotea/', None])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_publish(test_case, mocker, mqtt_adapter, topic_namespace):
    # pylint: disable=unused-argument
    async def mock_publish(topics, message, options=None, stash=True):
        pass

    mocker.patch('src.iotea.core.util.mqtt_client.MqttClient.publish', wraps=mock_publish)
    message = 'test message'
    topic = 'test'
    # test if publish options are passed to MqttClient correctly
    publish_options = ProtocolGateway.create_publish_options()
    publish_options.retain = not publish_options.retain
    publish_options.stash = not publish_options.stash

    await mqtt_adapter.publish(topic, message, publish_options)
    test_case.assertEqual(1, mqtt_adapter.client.publish.call_count)

    mqtt_adapter.client.publish.assert_called_once_with([prepend_namespace(topic_namespace, topic)],
                                                        message,
                                                        {'retain': publish_options.retain},
                                                        publish_options.stash)

    await mqtt_adapter.publish(topic, message, None)
    test_case.assertEqual(2, mqtt_adapter.client.publish.call_count)

    mqtt_adapter.client.publish.assert_called_with([prepend_namespace(topic_namespace, topic)],
                                                   message,
                                                   {'retain': ProtocolGateway.create_publish_options().retain},
                                                   ProtocolGateway.create_publish_options().stash)


@pytest.mark.parametrize("name", [None, 'MqttAdapterDisplayName'])
@pytest.mark.parametrize("topic_namespace", ['iotea/', None])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_subscribe_shared(test_case, mocker, mqtt_adapter, topic_namespace):
    # pylint: disable=unused-argument
    async def mock_subscribe(topic, callback, to_json=False):
        pass

    mocker.patch('src.iotea.core.util.mqtt_client.MqttClient.subscribe', wraps=mock_subscribe)
    topic = 'test'
    await mqtt_adapter.subscribe_shared('group', topic, lambda: {}, None)
    test_case.assertEqual(1, mqtt_adapter.client.subscribe.call_count)
    # check if new topic is constructed correctly
    if topic_namespace is None:
        topic_namespace = ''

    mqtt_adapter.client.subscribe.assert_called_once_with(f'$share/group/{prepend_namespace(topic_namespace, topic)}',
                                                          unittest.mock.ANY)

# pylint: disable=redefined-outer-name
def test_create_def_configuration(test_case):
    is_platform_protocol = True
    broker_url = 'mqtt://some-host:1886'
    default_config = MqttProtocolAdapter.create_default_configuration(is_platform_protocol=is_platform_protocol,
                                                                      broker_url=broker_url)
    test_case.assertEqual(is_platform_protocol, default_config['platform'])
    test_case.assertEqual(broker_url, default_config['config']['brokerUrl'])
    mqtt_adapter = MqttProtocolAdapter(default_config['config'])
    test_case.assertEqual(broker_url, mqtt_adapter.getId())
