##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import asyncio
import threading
import re
import logging
import json
import os
import queue
import time
from uuid import uuid4
from hbmqtt.client import MQTTClient
from hbmqtt.mqtt.constants import QOS_0

from ..constants import ONE_SECOND_MS

MAX_RECONNECT_RETRIES = 100000
MAX_RECONNECT_INTERVAL_S = 2
# Can be overriden by environment variable MQTT5_PROBE_TIMEOUT
DEFAULT_MQTT5_PROBE_TIMEOUT_MS = ONE_SECOND_MS
MQTT_MESSAGE_ENCODING = 'utf-8'

class CustomMqttClient(MQTTClient):
    def __init__(self, client_id=None, config=None, loop=None, on_reconnect=None):
        super(CustomMqttClient, self).__init__(client_id, config, loop)
        self.on_reconnect = on_reconnect

    async def reconnect(self, cleansession=True):
        code = await super(CustomMqttClient, self).reconnect(cleansession)

        if self.on_reconnect is not None:
            if asyncio.iscoroutinefunction(self.on_reconnect):
                await self.on_reconnect()
            else:
                self.on_reconnect()

        return code

class MqttClient:
    def __init__(self, connection_string, topic_ns=os.environ.get('MQTT_TOPIC_NS', None), check_mqtt5_compatibility=True, logger=None, client_id=None):
        if client_id is None:
            client_id = MqttClient.create_client_id('MqttClient')

        self.logger = logger

        if self.logger is None:
            self.logger = logging.getLogger(client_id)

        self.client = CustomMqttClient(client_id, on_reconnect=self.__on_reconnect)
        self.connection_string = connection_string
        self.client.config['reconnect_retries'] = MAX_RECONNECT_RETRIES
        self.client.config['reconnect_max_interval'] = MAX_RECONNECT_INTERVAL_S
        self.client_initialized = False
        self.subscriptions = []
        self.check_mqtt5_compatibility = check_mqtt5_compatibility
        self.is_mqtt5_compatible = False
        self.topic_ns = None

        if topic_ns is not None:
            if re.fullmatch(r'^[\/\w]+\/$', topic_ns) is not None:
                self.topic_ns = topic_ns
                self.logger.info('*****INFO***** Using topic namespace {}'.format(self.topic_ns))
            else:
                raise Exception('Given topic namespace {} is invalid. It has to have a trailing slash'.format(topic_ns))
        else:
            self.logger.warning('*****WARNING***** No topic namespace given. Tip: Also check all topics of your subscriptions and publications')

    async def get_client_async(self):
        if self.client_initialized:
            return self.client

        await self.__init(self.connection_string)
        self.client_initialized = True
        return self.client

    async def publish_json(self, topics, json_, options=None):
        if options is None:
            options = {}

        self.__validate_json(json_)

        await self.publish(topics, json.dumps(json_, separators=(',', ':')), options)

    async def publish(self, topics, message, options=None):
        if options is None:
            options = {}

        client = await self.get_client_async()

        if isinstance(topics, list) is False:
            topics = [topics]

        options = {**{'qos': QOS_0, 'retain': False}, **options}

        for topic in topics:
            prefixed_topic = self.__prefix_topic_ns(topic)
            self.logger.debug('Sending {} to {}'.format(message, prefixed_topic))
            await client.publish(prefixed_topic, message.encode(MQTT_MESSAGE_ENCODING), qos=options['qos'], retain=options['retain'])

    async def subscribe_json(self, topic, callback, qos=0):
        await self.subscribe(topic, callback, qos, True)

    async def subscribe(self, topic, callback, qos=QOS_0, to_json=False):
        client = await self.get_client_async()

        topic = self.__prefix_topic_ns(topic)

        subscription = Subscription(topic, callback, to_json, qos)

        self.subscriptions.append(subscription)

        await client.subscribe([(topic, qos)])

        self.logger.debug('Successfully subscribed to topic {}'.format(topic))

        return subscription

    async def disconnect(self):
        client = await self.get_client_async()
        await client.disconnect()

    async def unsubscribe(self, topics):
        client = await self.get_client_async()
        await self.client.unsubscribe(topics)

    @staticmethod
    def create_client_id(prefix):
        return '{}-{}'.format(prefix, str(uuid4())[:8])

    async def __init(self, connection_string):
        # Connecting
        self.logger.info('Connecting...')

        while True:
            try:
                await self.client.connect(connection_string, cleansession=True)
                break
            except:
                await asyncio.sleep(1)

        self.logger.info('Starting on_message co-routine...')

        # Start message coroutine in background
        self.on_message_future = asyncio.ensure_future(self.__run_on_message())

        # Check for mqtt5 here
        if self.check_mqtt5_compatibility and self.is_mqtt5_compatible is False:
            await self.__mqtt5_probe(self.client, int(os.environ.get('MQTT5_PROBE_TIMEOUT', DEFAULT_MQTT5_PROBE_TIMEOUT_MS)))

    def __prefix_topic_ns(self, topic):
        if self.topic_ns is None:
            return topic

        return re.sub(r'^(\$share\/[^\/]+\/)?(?:{})?(.+)'.format(self.topic_ns), r'\1' + self.topic_ns + r'\2', topic)

    async def __on_reconnect(self):
        for subscription in self.subscriptions:
            if subscription.should_unsubscribe:
                continue

            self.logger.debug('Resubscribing to {}'.format(subscription.topic))
            await self.client.subscribe([(subscription.topic, subscription.qos)])

    async def __mqtt5_probe(self, client, timeout_ms):
        self.logger.debug('Start MQTT5 Probing...')

        probe_uuid = str(uuid4())[0:8]

        publish_to = 'probe/{}'.format(probe_uuid)
        subscribe_to = self.__prefix_topic_ns('$share/{}/{}'.format(str(uuid4())[0:8], publish_to))
        # Prefix the publish to topic
        publish_to = self.__prefix_topic_ns(publish_to)

        probe_subscription = ProbeSubscription(subscribe_to)
        self.subscriptions.append(probe_subscription)

        self.logger.debug('Probe subscription is {}'.format(subscribe_to))

        await client.subscribe([
            (subscribe_to, QOS_0)
        ])

        self.logger.debug('Publishing probe to {}'.format(publish_to))

        await client.publish(publish_to, 'probe-{}'.format(probe_uuid).encode(MQTT_MESSAGE_ENCODING), qos=QOS_0)

        timeout_at_ms = time.time() * ONE_SECOND_MS + timeout_ms

        try:
            while probe_subscription.received_response is False:
                if time.time() * ONE_SECOND_MS > timeout_at_ms:
                    raise Exception('Probe on topic {} was not received on topic {}. An MQTT5 compilant broker is required'.format(publish_to, subscribe_to))

                await asyncio.sleep(0.1)
        finally:
            probe_subscription.unsubscribe()
            await self.client.unsubscribe([subscribe_to])
            self.subscriptions.remove(probe_subscription)

        self.is_mqtt5_compatible = True

    async def __run_on_message(self):
        while True:
            try:
                msg = await self.client.deliver_message()

                i = len(self.subscriptions) - 1

                while i >= 0:
                    subscription = self.subscriptions[i]
                    i -= 1
                    if subscription.should_unsubscribe:
                        await self.client.unsubscribe([subscription.topic])
                        self.subscriptions.remove(subscription)
                        continue

                    subscription.messages.put_nowait(msg)
            # pylint: disable=broad-except
            except Exception as err:
                self.logger.warning(err)
                await asyncio.sleep(1)

    def __validate_json(self, json_=None):
        if json_ is not None and (isinstance(json_, dict) or isinstance(json_, list)):
            return

        raise Exception('Given JSON document is neither a dictionary nor a list')

class NamedMqttClient(MqttClient):
    def __init__(self, name, connection_string, topic_ns=os.environ.get('MQTT_TOPIC_NS', None), check_mqtt5_compatibility=True):
        client_id = MqttClient.create_client_id('{}.MqttClient'.format(name))
        super(NamedMqttClient, self).__init__(connection_string, topic_ns, check_mqtt5_compatibility, logging.getLogger(client_id), client_id)

class Subscription:
    def __init__(self, topic, cb, to_json=False, qos=0):
        self.qos = qos
        self.topic = topic

        # Without shared subscription group
        # Mask $ for platform events or $SYS topics
        self.topic_regex = re.compile(
            '^{}$'.format(re.sub(r'^(\$share\/[^\/]+\/)', '', topic).replace('$', '\\$').replace('.', '\\.').replace('+', '[^\\/]+').replace('#', '.*'))
        )

        self.messages = queue.Queue(0)
        self.should_unsubscribe = False

        self.subscription_loop = asyncio.new_event_loop()
        self.subscription_thread = threading.Thread(daemon=True, target=self.__start_subscription, args=(self.subscription_loop, cb, to_json,))
        self.subscription_thread.start()

    def unsubscribe(self):
        self.should_unsubscribe = True
        self.messages.put_nowait(None)
        self.subscription_thread.join()

    def __start_subscription(self, subscription_loop, cb, to_json):
        asyncio.set_event_loop(subscription_loop)
        subscription_loop.run_until_complete(self.__wrap_subscription_callback(cb, to_json))

    async def __wrap_subscription_callback(self, callback, to_json=False):
        while self.should_unsubscribe is False:
            message = self.messages.get(block=True)

            if message is None:
                continue

            if self.topic_regex.fullmatch(message.topic) is None:
                continue

            decoded_message = message.publish_packet.payload.data.decode(MQTT_MESSAGE_ENCODING)

            if to_json:
                try:
                    decoded_message = json.loads(decoded_message)
                    self.__validate_json(decoded_message)
                # pylint: disable=broad-except
                except Exception:
                    # json.JSONDecodeError from json.loads or Exception from __validate_json
                    # Skip that message, since it's not a valid JSON document
                    continue

            if asyncio.iscoroutinefunction(callback):
                await asyncio.ensure_future(callback(decoded_message, message.topic))
            else:
                callback(decoded_message, message.topic)

    def __validate_json(self, json_=None):
        if json_ is not None and (isinstance(json_, dict) or isinstance(json_, list)):
            return

        raise Exception('Given JSON document is neither a dictionary nor a list')

class ProbeSubscription(Subscription):
    def __init__(self, topic):
        super(ProbeSubscription, self).__init__(topic, self.__on_probe_receive, False)
        self.received_response = False

    # pylint: disable=unused-argument
    async def __on_probe_receive(self, msg, topic):
        asyncio.get_event_loop().call_soon_threadsafe(self.__set_received, [True])

    def __set_received(self, received):
        self.received_response = received
