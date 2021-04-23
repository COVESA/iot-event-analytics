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
import functools
import os
import re
import logging
from uuid import uuid4

from .constants import TALENTS_DISCOVERY_TOPIC, DEFAULT_TYPE, DEFAULT_INSTANCE, MSG_TYPE_ERROR
from .rules import OrRules, Rule, OpConstraint, Constraint
from .util.mqtt_client import NamedMqttClient
from .util.logger import Logger
from .util.json_query import json_query_first
from .util.talent_io import TalentOutput
from .util import time_ms

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
        self.config = {}
        self.output_features = []

    def skip_cycle_check(self, value=True):
        if value != True:
            # Disable cycle check, do nothing
            # Already existing typeFeatures won't be overwritten
            return

        self.config['scc'] = True

    def skip_cycle_check_for(self, *args):
        if 'scc' in self.config and self.config['scc'] == True:
            # Skip, since cycle check is disabled anyway
            return

        if 'scc' not in self.config or isinstance(self.config['scc'], list) is False:
            self.config['scc'] = list(args)
            return

        # Ensure unique typeFeatures in Array
        self.config['scc'] = list(set([*self.config['scc'], *args]))

    def add_output(self, feature, metadata):
        self.output_features.append(OutputFeature(feature, metadata))

    def get_output_features(self, talent_id):
        outputs = {}
        return functools.reduce(lambda outputs, feature: feature.append_to(talent_id, outputs), self.output_features, outputs)

class Talent(IOFeatures):
    def __init__(self, talent_id, connection_string):
        super(Talent, self).__init__()
        # Unique for different talents
        # pylint: disable=invalid-name
        self.id = talent_id
        # Unique across all talents
        self.uid = Talent.create_uid(self.id)
        self.logger = logging.getLogger('Talent.{}'.format(self.uid))
        self.connection_string = connection_string
        self.mqtt_client = self.__create_mqtt_client('Talent.{}'.format(self.uid), connection_string)
        self.io_features = IOFeatures()
        self.deferred_calls = {}
        self.chnl = f'{self.id}.{str(uuid4())}'
        self.lock = threading.Lock()

        for callee in self.callees():
            self.skip_cycle_check_for(f'{DEFAULT_TYPE}.{callee}-out')

    async def start(self):
        await self.mqtt_client.subscribe_json(f'$share/{self.id}/{Talent.get_talent_topic(self.id)}', self.__on_event)
        await self.mqtt_client.subscribe_json(f'{Talent.get_talent_topic(self.id)}/{self.chnl}/+', self.__on_common_event)
        await self.mqtt_client.subscribe_json(f'$share/{self.id}/{TALENTS_DISCOVERY_TOPIC}', self.__on_discover)

        self.logger.info('Talent {} started successfully'.format(self.uid))

        while True:
            await asyncio.sleep(1)

    def callees(self):
        return []

    async def call(self, id, func, args, subject, return_topic, timeout_ms=10000):
        # Throws an exception if not available
        self.callees().index('{}.{}'.format(id, func))

        call_id = str(uuid4())

        ev = TalentOutput.create_for(
            subject,
            DEFAULT_TYPE,
            DEFAULT_INSTANCE,
            f'{id}.{func}-in', {
                'func': func,
                'args': args,
                'chnl': self.chnl,
                'call': call_id
            }
        )

        self.deferred_calls[call_id] = DeferredCall(call_id, timeout_ms)

        await self.mqtt_client.publish_json([return_topic], ev)

        self.logger.debug('Successfully sent function call')

        # pylint: disable=unused-variable
        done, pending = await asyncio.wait([self.deferred_calls[call_id].future])

        return list(done)[0].result()

    def get_rules(self):
        raise Exception('Override get_rules(self) and return an instance of Rules')

    def on_event(self, ev, evtctx):
        raise Exception('Override on_event(self, ev, evtctx)')

    def get_full_feature(self, talent_id, feature, _type=None):
        full_feature = f'{talent_id}.{feature}'

        if _type is None:
            return full_feature

        return f'{_type}.{full_feature}'

    async def publish_out_events(self, topic, out_events):
        if isinstance(out_events, list) is False:
            return

        with self.lock:
            for out_event in out_events:
                if 'whenMs' not in out_event:
                    out_event['whenMs'] = time_ms.time_ms()

                await self.mqtt_client.publish_json(topic, out_event)

    # pylint: disable=assignment-from-no-return, unused-argument
    async def __on_event(self, ev, topic):
        await self._process_event(ev, self.on_event)

    async def _process_event(self, ev, cb):
        evtctx = Logger.create_event_context(ev)

        if ev['msgType'] == MSG_TYPE_ERROR:
            self.logger.error('An error occured. Code {}'.format(ev['code']), extra=self.logger.create_extra(evtctx))
            return

        out_events = None

        if asyncio.iscoroutinefunction(cb):
            out_events = await cb(ev, evtctx)
        else:
            out_events = cb(ev, evtctx)

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
        await self.mqtt_client.publish_json(ev['returnTopic'], self.__create_discovery_response())

    def __create_discovery_response(self):
        rules = self._get_rules()

        def __on_rule(rule):
            if rule.constraint is None:
                return

            self.logger.info('  {}'.format(rule.constraint.to_string()))

        self.logger.info('{} depends on the following feature(s):'.format(self.id))
        rules.for_each(__on_rule)

        return {
            'id': self.id,
            'config': {
                **self.config,
                'outputs': self.get_output_features(self.id),
                'rules': rules.save()
            }
        }

    def _get_rules(self):
        """
        1) OR -> Triggerable Talent, which does not call any functions
             triggerRules
        2) OR -> Triggerable Talent, which calls one or more functions
             function result rules
             OR/AND [exclude function result rules]
               triggerRules
        """
        # pylint: disable=assignment-from-no-return
        trigger_rules = self.get_rules()

        function_result_rules = self._get_function_result_rules()

        if function_result_rules is None:
            # returns 1)
            return trigger_rules

        trigger_rules.exclude_on = list(map(lambda callee: f'{DEFAULT_TYPE}.{callee}-out', self.callees()))

        function_result_rules.add(trigger_rules)

        # returns 2)
        return function_result_rules

    def _get_function_result_rules(self):
        if len(self.callees()) == 0:
            return None

        return OrRules([
            # Ensure, that only the talent with the matching channel will receive the response
            # Since the full channel id is unique for a talent instance, this rule would fail, if there are multiple instances of a talent because it would only check for one talent here
            # -> The rule only checks the talent id prefix, which is common for all scaled Talent instances.
            # pylint: disable=anomalous-backslash-in-string
            *map(lambda callee: Rule(OpConstraint(f'{callee}-out', OpConstraint.OPS['REGEX'], '^\\/{}\\.[^\\/]+\\/.*'.format(self.id), DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW'], '/$tsuffix')), self.callees())
        ])

    def __create_mqtt_client(self, name, connection_string):
        return NamedMqttClient(name, connection_string, os.environ.get('MQTT_TOPIC_NS', None), True)

    @staticmethod
    def create_uid(prefix=None):
        unique_part = str(uuid4())[:8]

        if prefix is None:
            return unique_part

        return f'{prefix}-{unique_part}'

    @staticmethod
    def get_talent_topic(talent_id, suffix=''):
        return f'talent/{talent_id}/events{suffix}'
