##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import time
import asyncio
import functools
import os
import re
import logging
from uuid import uuid4

from .constants import TALENTS_DISCOVERY_TOPIC, DEFAULT_TYPE, MSG_TYPE_ERROR
from .rules import OrRules, Rule, OpConstraint, Constraint
from .mqtt_broker import NamedMqttBroker
from .logger import Logger
from .json_query import json_query_first

class DeferredCall:
    def __init__(self, call_id, timeout_ms):
        self.call_id = call_id
        self.loop = asyncio.get_event_loop()
        self.future = self.loop.create_future()
        self.timeout_sleep = None
        # Used to be compatible to Python 3.6.x
        asyncio.ensure_future(self.__schedule_timeout(timeout_ms))

    def resolve(self, result):
        self.timeout_sleep.cancel()

        if self.future.done():
            return

        # Called from another Thread
        self.loop.call_soon_threadsafe(self.future.set_result, result)

    def reject(self, err):
        self.timeout_sleep.cancel()

        if self.future.done():
            return

        # Called from another Thread
        self.loop.call_soon_threadsafe(self.future.set_exception, err)

    async def __schedule_timeout(self, timeout_ms):
        try:
            self.timeout_sleep = asyncio.ensure_future(asyncio.sleep(timeout_ms / 1000))
            await self.timeout_sleep

            if self.future.done():
                return

            # Called from another Thread
            self.loop.call_soon_threadsafe(self.future.set_exception, Exception('Timeout at calling function'))
        except:
            pass

class OutputFeature:
    def __init__(self, feature, metadata):
        self.feature = feature
        self.metadata = metadata

    def append_to(self, talent_id, feature_map):
        feature_map['.'.join([talent_id, self.feature])] = self.metadata
        return feature_map


class IOFeatures:
    def __init__(self):
        self.options = {}
        self.output_features = []

    def skip_cycle_check(self, value=True):
        self.options['scc'] = value

    def skip_cycle_check_for(self, *args):
        self.options['scc'] = list(args)

    def add_output(self, feature, metadata):
        self.output_features.append(OutputFeature(feature, metadata))

    def get_output_features(self, talent_id):
        outputs = {}
        return functools.reduce(lambda outputs, feature: feature.append_to(talent_id, outputs), self.output_features, outputs)


class Talent(IOFeatures):
    def __init__(self, talent_id, connection_string, disable_mqtt5_support=False):
        super(Talent, self).__init__()
        # Unique for different talents
        # pylint: disable=invalid-name
        self.id = talent_id
        # Unique across all talents
        self.uid = Talent.create_uid(self.id)
        self.logger = logging.getLogger('Talent.{}'.format(self.uid))
        self.connection_string = connection_string
        self.disable_mqtt5_support = disable_mqtt5_support
        self.broker = self.__create_message_broker('Talent.{}'.format(self.uid), connection_string)
        self.io_features = IOFeatures()
        self.deferred_calls = {}
        self.chnl = str(uuid4())

        self.logger.info('*****INFO***** Talent is deployable as remote?: {}'.format(self.is_remote()))

    async def start(self):
        event_subscription_topic = Talent.get_talent_topic(self.id)

        if self.disable_mqtt5_support is False:
            event_subscription_topic = '$share/{}/{}'.format(self.id, event_subscription_topic)

        await self.broker.subscribe_json(event_subscription_topic, self.__on_event)
        await self.broker.subscribe_json('{}/{}/+'.format(Talent.get_talent_topic(self.id), self.chnl), self.__on_common_event)
        await self.broker.subscribe_json('$share/{}/{}'.format(self.id, TALENTS_DISCOVERY_TOPIC), self.__on_discover)

        self.logger.info('Talent {} started successfully'.format(self.uid))

        while True:
            await asyncio.sleep(1)

    def callees(self):
        return []

    async def call(self, func_talent_id, func, args, subject, return_topic, timeout_ms=10000):
        # Throws an exception if not available
        self.callees().index('{}.{}'.format(func_talent_id, func))

        call_id = str(uuid4())

        invocation_event = {
            'subject': subject,
            'type': DEFAULT_TYPE,
            'feature': '{}.{}-in'.format(func_talent_id, func),
            'value': {
                'func': func,
                'args': args,
                'chnl': self.chnl,
                'call': call_id
            },
            'whenMs': round(time.time() * 1000)
        }

        self.deferred_calls[call_id] = DeferredCall(call_id, timeout_ms)

        await self.broker.publish_json([return_topic], invocation_event)

        self.logger.debug('Successfully sent function call')

        # pylint: disable=unused-variable
        done, pending = await asyncio.wait([self.deferred_calls[call_id].future])

        return list(done)[0].result()

    def is_remote(self):
        return False

    def get_rules(self):
        raise Exception('Override get_rules(self) and return an instance of Rules')

    def on_event(self, ev, evtctx):
        raise Exception('Override on_event(self, ev, evtctx)')

    def get_full_feature(self, talent_id, feature, _type=None):
        full_feature = '{}.{}'.format(talent_id, feature)

        if _type is None:
            return full_feature

        return '{}.{}'.format(_type, full_feature)

    async def publish_out_events(self, topic, out_events):
        if isinstance(out_events, list) is False:
            return

        for out_event in out_events:
            if 'whenMs' not in out_event:
                out_event['whenMs'] = round(time.time_ns() / 1000)

            await self.broker.publish_json(topic, out_event)

    # pylint: disable=assignment-from-no-return, unused-argument
    async def __on_event(self, ev, topic):
        evtctx = Logger.create_event_context(ev)

        if ev['msgType'] == MSG_TYPE_ERROR:
            self.logger.error('An error occured. Code {}'.format(ev['code']), extra=self.logger.create_extra(evtctx))
            return

        out_events = None

        if asyncio.iscoroutinefunction(self.on_event):
            out_events = await self.on_event(ev, evtctx)
        else:
            out_events = self.on_event(ev, evtctx)

        await self.publish_out_events(ev['returnTopic'], out_events)

    async def __on_common_event(self, ev, topic):
        # self.logger.debug('Received common event {} at topic {}'.format(json.dumps(ev), topic))

        suffix_match = re.fullmatch('^.*\\/([^\\/]+)$', topic)

        if suffix_match is None:
            return

        call_id = suffix_match[1]

        if call_id not in self.deferred_calls:
            # Talent is not waiting for this response
            self.logger.debug('Deferred call with id {} could not be found'.format(call_id))
            return

        deferred_call = self.deferred_calls[call_id]

        del self.deferred_calls[call_id]

        self.logger.debug('Deferred call found with id {}'.format(call_id))

        value = json_query_first(ev['value'], ev['value']['$vpath'])['value']

        if ev['msgType'] == MSG_TYPE_ERROR:
            deferred_call.reject(Exception(value))
            return

        deferred_call.resolve(value)

    async def __on_discover(self, ev, topic):
        rules = self.__get_rules()

        def __on_rule(rule):
            if rule.constraint is None:
                return

            self.logger.info('  {}'.format(rule.constraint.to_string()))

        self.logger.info('{} depends on the following feature(s):'.format(self.id))
        rules.for_each(__on_rule)

        discovery_response = {
            'id': self.id,
            'remote': self.is_remote(),
            'options': self.options,
            'outputs': self.get_output_features(self.id),
            'rules': rules.save()
        }

        await self.broker.publish_json(ev['returnTopic'], discovery_response)

    def __get_rules(self):
        # pylint: disable=assignment-from-no-return
        rules = self.get_rules()

        if len(self.callees()) == 0:
            return rules

        return OrRules([
            # pylint: disable=anomalous-backslash-in-string
            # Ensure, that only the talent with the matching channel will receive the response
            *map(lambda callee: Rule(OpConstraint('{}-out'.format(callee), OpConstraint.OPS['REGEX'], '^\/{}\/.*'.format(self.chnl), DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW'], '/$tsuffix')), self.callees()),
            rules
        ])

    def __create_message_broker(self, name, connection_string):
        check_mqtt5_compatibility = True

        if self.disable_mqtt5_support:
            if self.is_remote() is False:
                raise Exception('Disabling MQTT5 support is only supported for remote talents')

            check_mqtt5_compatibility = False

        return NamedMqttBroker(name, connection_string, os.environ.get('MQTT_TOPIC_NS', None), check_mqtt5_compatibility)

    @staticmethod
    def create_uid(prefix=None):
        unique_part = str(uuid4())[:8]

        if prefix is None:
            return unique_part

        return '{}-{}'.format(prefix, unique_part)

    @staticmethod
    def get_talent_topic(talent_id, is_remote=False, suffix=''):
        topic = 'talent/{}/events{}'.format(talent_id, suffix)

        if is_remote:
            return 'remote/{}'.format(topic)

        return topic
