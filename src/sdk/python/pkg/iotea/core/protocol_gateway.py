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
import importlib
import json

from .util.json_model import JsonModel


class ProtocolGateway:
    def __init__(self, protocol_gateway_config, display_name, use_platform_protocol_only=False):
        ProtocolGateway.validate_configuration(protocol_gateway_config, use_platform_protocol_only)
        pg_config = JsonModel(protocol_gateway_config)
        self.use_platform_protocol_only = use_platform_protocol_only
        self.adapters = []

        for adapter_config in pg_config.get('adapters'):
            adapter_config_model = JsonModel(adapter_config)
            is_platform_protocol = adapter_config_model.get('platform', False)

            if self.use_platform_protocol_only and not is_platform_protocol:
                # Skip all non-platform protocols
                continue

            module = importlib.import_module(adapter_config_model.get('module.name'), package=__package__)
            a_class = getattr(module, adapter_config_model.get('module.class'))
            instance = a_class(adapter_config_model.get('config'), display_name)

            self.adapters.append(Adapter(instance, is_platform_protocol))

    async def publish(self, topic, message, publish_options=None):
        if publish_options is None:
            publish_options = ProtocolGateway.create_publish_options()

        publish_to_platform_protocol_only = publish_options.platform_protocol_only

        if publish_to_platform_protocol_only is None:
            # Apply global settings, if nothing is given
            publish_to_platform_protocol_only = self.use_platform_protocol_only

        self.__validate_platform_protocol_usage(publish_to_platform_protocol_only)

        for adapter in self.adapters:
            if publish_to_platform_protocol_only is False or adapter.is_platform_protocol:
                if publish_options.adapter_id is None or publish_options.adapter_id is adapter.id:
                    await adapter.instance.publish(topic, message, publish_options)

    def publish_json(self, topic, json_o, publish_options=None):
        return self.publish(topic, json.dumps(json_o, separators=(',', ':')), publish_options)

    # Callback needs to accept (ev, topic, adapter_id)
    async def subscribe(self, topic, callback, subscribe_options=None):
        if subscribe_options is None:
            subscribe_options = ProtocolGateway.create_subscribe_options()
        subscribe_to_platform_protocol_only = subscribe_options.platform_protocol_only

        if subscribe_to_platform_protocol_only is None:
            # Apply global settings, if nothing is given
            subscribe_to_platform_protocol_only = self.use_platform_protocol_only

        self.__validate_platform_protocol_usage(subscribe_to_platform_protocol_only)

        for adapter in self.adapters:
            if subscribe_to_platform_protocol_only is False or adapter.is_platform_protocol:
                if subscribe_options.adapter_id is None or subscribe_options.adapter_id is adapter.id:
                    if asyncio.iscoroutinefunction(callback):
                        async def callback_wrapper(ev, _topic, adapter_id=adapter.id):
                            await callback(ev, _topic, adapter_id)

                        cb = callback_wrapper
                    else:
                        def callback_wrapper(ev, _topic, adapter_id=adapter.id):
                            callback(ev, _topic, adapter_id)

                        cb = callback_wrapper

                    await adapter.instance.subscribe(topic, cb, subscribe_options)

    # Callback needs to accept (ev, topic, adapter_id)
    async def subscribe_json(self, topic, callback, subscribe_options=None):
        return await self.subscribe(topic, ProtocolGateway.__json_parse_wrapper(callback), subscribe_options)

    # Callback needs to accept (ev, topic, adapter_id)
    async def subscribe_shared(self, group, topic, callback, subscribe_options=None):
        if subscribe_options is None:
            subscribe_options = ProtocolGateway.create_subscribe_options()

        subscribe_to_platform_protocol_only = subscribe_options.platform_protocol_only

        if subscribe_to_platform_protocol_only is None:
            # Apply global settings, if nothing is given
            subscribe_to_platform_protocol_only = self.use_platform_protocol_only

        self.__validate_platform_protocol_usage(subscribe_to_platform_protocol_only)

        for adapter in self.adapters:
            if subscribe_to_platform_protocol_only is False or adapter.is_platform_protocol:
                if subscribe_options.adapter_id is None or subscribe_options.adapter_id is adapter.id:
                    if asyncio.iscoroutinefunction(callback):
                        async def callback_wrapper(ev, _topic, adapter_id=adapter.id):
                            await callback(ev, _topic, adapter_id)

                        cb = callback_wrapper
                    else:
                        def callback_wrapper(ev, _topic, adapter_id=adapter.id):
                            callback(ev, _topic, adapter_id)

                        cb = callback_wrapper

                    await adapter.instance.subscribe_shared(group, topic, cb, subscribe_options)

    @staticmethod
    def __json_parse_wrapper(callback):
        if asyncio.iscoroutinefunction(callback):
            async def callback_wrapper(stringified_json, _topic, adapter_id):
                try:
                    await callback(ProtocolGateway.__try_parse_json(stringified_json), _topic, adapter_id)
                except Exception:
                    # Could not parse Json from incoming message
                    pass

            cb = callback_wrapper
        else:
            def callback_wrapper(stringified_json, _topic, adapter_id):
                try:
                    callback(ProtocolGateway.__try_parse_json(stringified_json), _topic, adapter_id)
                except Exception:
                    # Could not parse Json from incoming message
                    pass

            cb = callback_wrapper
        return cb

    # Callback needs to be (ev, topic, adapter.id) => {}
    async def subscribe_json_shared(self, group, topic, callback, subscribe_options=None):
        return await self.subscribe_shared(group, topic, ProtocolGateway.__json_parse_wrapper(callback),
                                           subscribe_options)

    @staticmethod
    def __try_parse_json(stringified_json):
        return json.loads(stringified_json)

    def __validate_platform_protocol_usage(self, use_platform_protocol_only):
        if self.use_platform_protocol_only and use_platform_protocol_only is False:
            raise Exception(
                'Gateway is configured to only use the provided platform protocol. Runtime request for all protocols given.')

    @staticmethod
    def validate_configuration(protocol_gateway_config, use_platform_protocol_only=False):
        if 'adapters' not in protocol_gateway_config:
            raise Exception(
                'Invalid ProtocolGateway configuration. Field "adapters" is missing!')
        adapters = protocol_gateway_config['adapters']
        if not isinstance(adapters, list):
            raise Exception(
                'Invalid ProtocolGateway configuration. Field "adapters" needs to be an array. Found {type(adapters)}')

        # Check if platform adapter is just used once
        platform_adapter_count = len([adapter for adapter in adapters if JsonModel(adapter).get('platform', False)])

        if platform_adapter_count > 1:
            raise Exception('Invalid ProtocolGateway configuration. More than one platform adapter found')

        if use_platform_protocol_only and platform_adapter_count == 0:
            raise Exception('Should use platform protocol only, but no platform adapter found')

    # public function, not used internally
    @staticmethod
    def has_platform_adapter(protocol_gateway_config):
        ProtocolGateway.validate_configuration(protocol_gateway_config)
        for adapter in protocol_gateway_config['adapters']:
            if JsonModel(adapter).get('platform', False):
                return True
        return False

    @staticmethod
    def get_adapter_count(protocol_gateway_config):
        ProtocolGateway.validate_configuration(protocol_gateway_config)
        return len(protocol_gateway_config['adapters'])

    @staticmethod
    def create_subscribe_options(platform_protocol_only=None, adapter_id=None):
        return SubscribeOptions(platform_protocol_only, adapter_id)

    @staticmethod
    def create_publish_options(platform_protocol_only=None, adapter_id=None):
        return PublishOptions(platform_protocol_only, adapter_id)

    @staticmethod
    def create_default_configuration(protocol_adapters_config):
        return {'adapters': protocol_adapters_config}


class PubSubOptions:
    def __init__(self, platform_protocol_only=None, adapter_id=None):
        self.platform_protocol_only = platform_protocol_only
        self.adapter_id = adapter_id


class PublishOptions(PubSubOptions):
    def __init__(self, platform_protocol_only, adapter_id):
        super().__init__(platform_protocol_only, adapter_id)
        # Retain this published message
        self.retain = False
        # If client / broker is offline, keep these messages stashed until it's online again. Then republish
        self.stash = True


class SubscribeOptions(PubSubOptions):
    def __init__(self, platform_protocol_only, adapter_id):
        super().__init__(platform_protocol_only, adapter_id)


class Adapter:
    def __init__(self, instance, is_platform_protocol):
        self.instance = instance
        self.is_platform_protocol = is_platform_protocol
        self.id = instance.getId()
