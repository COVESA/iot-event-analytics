import asyncio
import json
import unittest
from unittest import TestCase

import pytest

from src.iotea.core.protocol_gateway import ProtocolGateway
from src.iotea.core.util.mqtt_client import MqttProtocolAdapter

# timeout for blocking operations on asyncio synchronization primitives
TIMEOUT = 10


@pytest.fixture
def test_case():
    return TestCase()


@pytest.fixture
def protocol_gateway():
    mqtt_def_config = MqttProtocolAdapter.create_default_configuration(is_platform_protocol=True)
    protocol_gateway_config = ProtocolGateway.create_default_configuration([mqtt_def_config])
    return ProtocolGateway(protocol_gateway_config, 'TestProtocolGateway')


# pylint: disable=unused-argument
async def mock_publish(topic, message, publish_options=None, force_wait=False):
    pass


# pylint: disable=redefined-outer-name
def get_async_callback(test_case, callback_invoked_event, test_message, test_topic, default_broker_url):
    # Because of this issue https: // bugs.python.org / issue26140
    # present in <= python 3.7:  asyncio.iscoroutinefunction(Mock(async_func)) fails
    # and as AsyncMock is not available in 3.7, we need to redefine the callback functions
    async def async_callback(message, topic, adapter_id):
        get_sync_callback(test_case, callback_invoked_event, test_message, test_topic, default_broker_url)(message,
                                                                                                           topic,
                                                                                                           adapter_id)

    return async_callback


# pylint: disable=redefined-outer-name
def get_sync_callback(test_case, callback_invoked_event, test_message, test_topic, default_broker_url):
    def sync_callback(message, topic, adapter_id):
        callback_invoked_event.set()
        if isinstance(message, str):
            message = json.loads(message)
        test_case.assertEqual(json.loads(test_message), message)
        test_case.assertEqual(test_topic, topic)
        test_case.assertEqual(default_broker_url, adapter_id)

    return sync_callback


@pytest.mark.parametrize("test_message", ['{"json": "test message"}'])
@pytest.mark.parametrize("force_wait", [True, False])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_subscribe_shared(test_case, mocker, protocol_gateway, test_message, force_wait):
    default_broker_url = 'mqtt://localhost:1883'
    test_topic = 'test'
    group = 'sub_shared'
    callback_invoked_event = asyncio.Event()

    # pylint: disable=unused-argument
    async def mock_subscribe_shared(group, topic, cb, subscribe_options=None):
        if asyncio.iscoroutinefunction(cb):
            await cb(test_message, topic)
        else:
            cb(test_message, topic)

    mocker.patch('src.iotea.core.util.mqtt_client.MqttProtocolAdapter.subscribe_shared',
                 wraps=mock_subscribe_shared)

    pg_subscribe_methods = [protocol_gateway.subscribe_shared, protocol_gateway.subscribe_json_shared]

    callbacks = [get_async_callback(test_case, callback_invoked_event, test_message, test_topic, default_broker_url),
                 get_sync_callback(test_case, callback_invoked_event, test_message, test_topic, default_broker_url)]

    for pg_subscribe in pg_subscribe_methods:
        for callback in callbacks:
            subscribe_options = ProtocolGateway.create_subscribe_options()
            await pg_subscribe(group, test_topic, callback, subscribe_options=subscribe_options,
                               force_wait=force_wait)
            protocol_gateway.adapters[0].instance.subscribe_shared.assert_called_with(group, test_topic,
                                                                                      unittest.mock.ANY,
                                                                                      subscribe_options)

            # the subscription is not awaited but sent to event loop, so wait to get the job done
            if not force_wait:
                await asyncio.wait_for(callback_invoked_event.wait(), TIMEOUT)
            # as Mock(async_callback) does not pass the test iscoroutingfunction,
            # we need this hack to manually check if the callback has been invoked
            test_case.assertEqual(True, callback_invoked_event.is_set(),
                                  f'Subscribe callback {callback} was not invoked from {pg_subscribe}!')
            callback_invoked_event.clear()


@pytest.mark.parametrize("test_message", ['{"json": "test message"}'])
@pytest.mark.parametrize("force_wait", [True, False])
@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_subscribe(test_case, mocker, protocol_gateway, test_message, force_wait):
    default_broker_url = 'mqtt://localhost:1883'
    test_topic = 'test'
    callback_invoked_event = asyncio.Event()

    # pylint: disable=unused-argument
    async def mock_subscribe(topic, cb, subscribe_options=None):
        if asyncio.iscoroutinefunction(cb):
            await cb(test_message, topic)
        else:
            cb(test_message, topic)

    mocker.patch('src.iotea.core.util.mqtt_client.MqttProtocolAdapter.subscribe', wraps=mock_subscribe)
    pg_subscribe_methods = [protocol_gateway.subscribe, protocol_gateway.subscribe_json]

    callbacks = [get_async_callback(test_case, callback_invoked_event, test_message, test_topic, default_broker_url),
                 get_sync_callback(test_case, callback_invoked_event, test_message, test_topic, default_broker_url)]

    for pg_subscribe in pg_subscribe_methods:
        for callback in callbacks:
            subscribe_options = ProtocolGateway.create_subscribe_options()

            await pg_subscribe(test_topic, callback, subscribe_options=subscribe_options, force_wait=force_wait)
            protocol_gateway.adapters[0].instance.subscribe.assert_called_with(test_topic, unittest.mock.ANY,
                                                                               subscribe_options)

            # the subscription is not awaited but sent to event loop, so wait to get the job done
            if not force_wait:
                await asyncio.wait_for(callback_invoked_event.wait(), TIMEOUT)
            # as Mock(async_callback) does not pass the test iscoroutingfunction,
            # we need this hack to manually check if the callback has been invoked
            test_case.assertEqual(True, callback_invoked_event.is_set(),
                                  f'Subscribe callback {callback} was not invoked from {pg_subscribe}!')
            callback_invoked_event.clear()


# pylint: disable=redefined-outer-name
def test_has_platform_adapter(test_case, protocol_gateway):
    pg_config = ProtocolGateway.create_default_configuration([{'platform': True}])
    test_case.assertEqual(True, protocol_gateway.has_platform_adapter(pg_config),
                          'ProtocolGateway should have a platform adapter!')

    pg_config = ProtocolGateway.create_default_configuration([{}])
    test_case.assertEqual(False, protocol_gateway.has_platform_adapter(pg_config),
                          'ProtocolGateway should not have a platform adapter!')


@pytest.mark.parametrize('test_message', ['test message', {'json': 'test message'}])
@pytest.mark.parametrize('force_wait', [True, False])
@pytest.mark.asyncio
async def test_publish(test_case, mocker, protocol_gateway, test_message, force_wait):
    test_topic = 'test'

    mocker.patch('src.iotea.core.util.mqtt_client.MqttProtocolAdapter.publish', wraps=mock_publish)

    publish_options = ProtocolGateway.create_publish_options()
    if isinstance(test_message, str):
        await protocol_gateway.publish(test_topic, test_message, publish_options, force_wait=force_wait)
        test_case.assertEqual(1, protocol_gateway.adapters[0].instance.publish.call_count)
        protocol_gateway.adapters[0].instance.publish.assert_called_once_with(test_topic, test_message, publish_options)
    else:
        await protocol_gateway.publish_json(test_topic, test_message, publish_options, force_wait=force_wait)
        test_case.assertEqual(1, protocol_gateway.adapters[0].instance.publish.call_count)
        protocol_gateway.adapters[0].instance.publish.assert_called_once_with(test_topic, json.dumps(test_message,
                                                                                                     separators=(
                                                                                                         ',', ':')),
                                                                              publish_options)


@pytest.mark.asyncio
# pylint: disable=redefined-outer-name
async def test_publish_options(test_case, mocker, protocol_gateway):
    test_topic = 'test'
    test_message = 'test message'
    mocker.patch('src.iotea.core.util.mqtt_client.MqttProtocolAdapter.publish', wraps=mock_publish)
    # test publish with None pub options
    await protocol_gateway.publish(test_topic, test_message, None)
    test_case.assertEqual(1, protocol_gateway.adapters[0].instance.publish.call_count)
    protocol_gateway.adapters[0].instance.publish.assert_called_once_with(test_topic, test_message, unittest.mock.ANY)
    args, _ = protocol_gateway.adapters[0].instance.publish.call_args
    # check that public options is not None
    test_case.assertIsNotNone(args[2])

    # test publish to platform adapter
    publish_options = ProtocolGateway.create_publish_options(True, None)
    await protocol_gateway.publish(test_topic, test_message, publish_options)
    test_case.assertEqual(2, protocol_gateway.adapters[0].instance.publish.call_count)
    protocol_gateway.adapters[0].instance.publish.assert_called_with(test_topic, test_message, publish_options)


@pytest.mark.asyncio
async def test_subscribe_options(test_case, mocker, protocol_gateway):
    test_topic = 'test'

    # pylint: disable=unused-argument
    async def mock_subscribe(topic, cb, subscribe_options=None):
        pass

    mocker.patch('src.iotea.core.util.mqtt_client.MqttProtocolAdapter.subscribe', wraps=mock_subscribe)
    # test with None sub options
    await protocol_gateway.subscribe(test_topic, lambda: {}, subscribe_options=None)
    test_case.assertEqual(1, protocol_gateway.adapters[0].instance.subscribe.call_count)
    protocol_gateway.adapters[0].instance.subscribe.assert_called_with(test_topic, unittest.mock.ANY, unittest.mock.ANY)
    args, _ = protocol_gateway.adapters[0].instance.subscribe.call_args
    subscribe_options = args[2]
    test_case.assertIsNotNone(subscribe_options)

    # test with custom options: subscribe with platform protocol adapter
    subscribe_options = ProtocolGateway.create_subscribe_options(True, None)
    await protocol_gateway.subscribe(test_topic, lambda: {}, subscribe_options=subscribe_options)
    test_case.assertEqual(2, protocol_gateway.adapters[0].instance.subscribe.call_count)
    protocol_gateway.adapters[0].instance.subscribe.assert_called_with(test_topic, unittest.mock.ANY, subscribe_options)


def test_validate_configuration(test_case):
    with pytest.raises(Exception) as exc_info:
        ProtocolGateway.validate_configuration('bad configuration')
    test_case.assertEqual('Invalid ProtocolGateway configuration. Field "adapters" is missing!', str(exc_info.value))

    # adapters expected in configuration
    with pytest.raises(Exception) as exc_info:
        ProtocolGateway.validate_configuration({'no-adapters': 'present'})
    test_case.assertEqual('Invalid ProtocolGateway configuration. Field "adapters" is missing!', str(exc_info.value))

    # adapters must be a list
    with pytest.raises(Exception) as exc_info:
        ProtocolGateway.validate_configuration({'adapters': 'is not a list'})
    test_case.assertEqual(
        f'Invalid ProtocolGateway configuration. Field "adapters" needs to be an array. Found <class \'str\'>',
        str(exc_info.value))

    # more than one platform adapter not allowed
    with pytest.raises(Exception) as exc_info:
        ProtocolGateway.validate_configuration({'adapters': [{'platform': True}, {'platform': True}]})
    test_case.assertEqual(f'Invalid ProtocolGateway configuration. More than one platform adapter found',
                          str(exc_info.value))

    # no platform adapter available, while required
    with pytest.raises(Exception) as exc_info:
        ProtocolGateway.validate_configuration({'adapters': [{'platform': False}, {'no-platform-tag': True}]},
                                               use_platform_protocol_only=True)
    test_case.assertEqual(f'Should use platform protocol only, but no platform adapter found',
                          str(exc_info.value))


@pytest.mark.asyncio
async def test_validate_platform_protocol_usage(test_case):
    mqtt_def_config = MqttProtocolAdapter.create_default_configuration(is_platform_protocol=True)
    protocol_gateway_config = ProtocolGateway.create_default_configuration([mqtt_def_config])
    protocol_gateway = ProtocolGateway(protocol_gateway_config, 'TestProtocolGateway', use_platform_protocol_only=True)
    publish_options = ProtocolGateway.create_publish_options(platform_protocol_only=False)
    with pytest.raises(Exception) as exc_info:
        await protocol_gateway.publish("test_topic", "test message", publish_options)
    test_case.assertEqual(
        f'Gateway is configured to only use the provided platform protocol. Runtime request for all protocols given.',
        str(exc_info.value))


def test_init_protocol_gateway(test_case):
    # create a configuration with 2 adapters: 1 platform and 1 not, the PG (use_platform_protocol_only=True) should filter only 1 of them
    mqtt_def_config_1 = MqttProtocolAdapter.create_default_configuration(is_platform_protocol=False)
    mqtt_def_config_2 = MqttProtocolAdapter.create_default_configuration(is_platform_protocol=True)
    protocol_gateway_config = ProtocolGateway.create_default_configuration([mqtt_def_config_1, mqtt_def_config_2])
    protocol_gateway = ProtocolGateway(protocol_gateway_config, 'TestProtocolGateway', use_platform_protocol_only=True)

    test_case.assertEqual(1, len(protocol_gateway.adapters))


@pytest.mark.parametrize('test_message', ['wrong json message'])
@pytest.mark.asyncio
async def test_subscribe_json_fail(test_case, mocker, protocol_gateway, test_message):
    callback_invoked = False

    def sync_callback(message, topic):
        nonlocal callback_invoked
        callback_invoked = True

    async def async_callback(message, topic):
        sync_callback(message, topic)

    async def mock_subscribe(topic, cb, subscribe_options=None):
        if asyncio.iscoroutinefunction(cb):
            await cb(test_message, topic)
        else:
            cb(test_message, topic)

    mocker.patch('src.iotea.core.util.mqtt_client.MqttProtocolAdapter.subscribe', wraps=mock_subscribe)

    callbacks = [sync_callback, async_callback]
    for i, callback in enumerate(callbacks, start=1):
        await protocol_gateway.subscribe_json('test', callback, force_wait=True)
        test_case.assertEqual(i, protocol_gateway.adapters[0].instance.subscribe.call_count)
        test_case.assertEqual(False, callback_invoked, 'Callback should not be called, wrong json format!')
        callback_invoked = False
