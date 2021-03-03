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
import json
import os
import paho.mqtt.client as mqtt
import sys
import threading
import time
from functools import partial

# Only for MOCK
from dapr.clients.grpc._response import InvokeServiceResponse

# dapr related modules
import grpc
from cloudevents.sdk.event import v1
from dapr.clients import DaprClient
from dapr.conf import settings
from dapr.ext.grpc import InvokeServiceRequest
from typing import Dict, Optional
from concurrent import futures

# IoTEA realted modules
from iotea.core.constants import PLATFORM_EVENTS_TOPIC, PLATFORM_EVENT_TYPE_SET_RULES, PLATFORM_EVENT_TYPE_UNSET_RULES, INGESTION_TOPIC
from iotea.core.logger import Logger
from iotea.core.rules import OrRules, Rule, Constraint, OpConstraint
from iotea.core.talent_func import FunctionTalent

# Logging
import logging
logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

# Talent related
from helper.dapr_grpc import DaprGRPC
from config.constants import *
os.environ["MQTT_TOPIC_NS"] = "iotea/"

CONTENT_TYPE_JSON = "application/json"
INGESTION_TOPIC_WITH_PREFIX = "{}{}".format(os.environ["MQTT_TOPIC_NS"], INGESTION_TOPIC)
TIMEOUT_IOTEA_RPC_CALL = 3000

USE_DAPR_SERVICE_MOCK = False  # For testing only
TEST_IOTEA_RPC_INTERNAL = False # For testing only

class DaprAdapter(FunctionTalent):
# Public Methods

    def __init__(self, mqtt_host, mqtt_port, **kwargs):
        # Build connection string for iotea mqtt client
        connection_string = "mqtt:://{}:{}".format(mqtt_host, mqtt_port)

        super(DaprAdapter, self).__init__("dapr_adapter", connection_string)

        # Store loop for thread bridging
        self.loop = asyncio.get_running_loop()

        # Use paho mqtt client for publishing to avoid thread issues
        # which are happen if dapr events arrived by a different thread and
        # not using asyncio
        self.publish_mqtt_client = mqtt.Client(client_id=self.id)
        self.publish_mqtt_client.connect(mqtt_host, mqtt_port)

        self.logger = logging.getLogger(self.id)
        self.logger.warning("RPC mappings are not implemented!")

        # Create loop for publish_json
        self.loop = asyncio.get_event_loop()
        self.publish_loop = asyncio.new_event_loop()
        t = threading.Thread(target=self.publish_loop.run_forever)
        t.start()

        # Instanciate class members for pub-sub mapping
        self.dapr_calles = list()
        self.dapr_subscriptions = dict()
        self.provided_features = dict()
        self.active_feature_subscriptions = dict()
        self.last_active_feature_subscriptions = None
        self.subscription_rules = list()
        self.iotea_subscriptions = dict()

        # Instanciate class members for rpc map
        self.dapr_rpc_bindings = dict()
        self.iotea_rpc_bindings = dict()

        # Initial servicer and server
        self.logger.info("Start dapr GPRC server")
        self._servicer = DaprGRPC()
        self._server = self._servicer.create_server(**kwargs)

        # Init mappings (dapr <> iotea)
        self.__init_pub_sub_mappings()
        self.__init_rpc_mappings()

        # Instanciate DarpClient to publish events
        self.dapr_client = DaprClient()

        # Subscribe dapr topics
        for pubsub_topic in self.dapr_subscriptions:
           sub = self.dapr_subscriptions[pubsub_topic][0]  # Use the first one for pubsub and topic
           self.__subscribe(sub[DAPR_FIELD][PUBSUB_NAME_FIELD], sub[DAPR_FIELD][TOPIC_FIELD])

    def callees(self):
        # Dapr calles are subscribed functions of dapr
        return self.dapr_calles

    def get_rules(self):
        # Handle subscriptions via OrRule to subscribe them for forwarding
        return OrRules([*self.subscription_rules, super(DaprAdapter, self).get_rules()])

    async def on_event(self, ev, evtctx):
        # Check and forward rpc events
        if await self.__check_rpc(ev, evtctx):
            return

        # Forward iotea events if subscribed
        self.logger.debug("IoTEA event arrived {}".format(ev))

        if ev["feature"] in self.iotea_subscriptions:
            self.logger.debug("Mapping to dapr found {}".format(self.iotea_subscriptions[ev[FEATURE_FIELD]]))
            mapping = self.iotea_subscriptions[ev[FEATURE_FIELD]]

            self.logger.info("Publish event in iotea for feature {}".format(ev["feature"]))

            self.dapr_client.publish_event(pubsub_name = mapping[DAPR_FIELD][PUBSUB_NAME_FIELD],
                                             topic = mapping[DAPR_FIELD][TOPIC_FIELD],
                                             data = json.dumps(ev))
        else:
            self.logger.warning("No iotea subscriptions for event {}".format(json.dumps(ev)))

    async def start(self, dapr_app_port):
        # Run dapr connector
        self.__run(dapr_app_port)

        # Run publish mqtt client thread
        self.logger.info("Start publish thread (paho mqtt client)")
        self.publish_mqtt_thread = threading.Thread(target=self.publish_mqtt_client.loop_forever)
        self.publish_mqtt_thread.start()

        # Register to platform events to check requested features (via bindings)
        await self.broker.subscribe_json(PLATFORM_EVENTS_TOPIC, self.__onPlatformEvent)
        await super(DaprAdapter,self).start()

    def stop(self) -> None:
        self.logger.info("Join publish mqtt thread")
        self.publish_mqtt_thread.join()

        self.logger.info("Stop dapr GPRC server")
        self._server.stop(0)

# Private Methods

    def __init_pub_sub_mappings(self):
        self.logger.debug("Initialize pub-sub mappings")

        try:
            sub_file = open("config/pub_sub_mapping.json")
            sub_json = json.load(sub_file)
            dapr_in = sub_json[IN_FIELD]

            for mapping in dapr_in:
                pubsub_topic = "{}.{}".format(mapping[DAPR_FIELD][PUBSUB_NAME_FIELD], mapping[DAPR_FIELD][TOPIC_FIELD])
                feature_type = mapping[IOTEA_FIELD][TYPE_FILED]
                feature = mapping[IOTEA_FIELD][FEATURE_FIELD]

                if feature_type is None:
                    # Set own class as type and add output to platform
                    feature_type = self.id
                    feature = mapping[IOTEA_FIELD][FEATURE_FIELD]

                    self.logger.info("Add output for feature {}".format(feature))
                    meta = dict()
                    meta["description"] = mapping[IOTEA_FIELD][DESCRIPTION_FIELD]
                    meta["encoding"] = mapping[IOTEA_FIELD][ENCODING_FIELD]
                    meta["unit"] = mapping[IOTEA_FIELD][UNIT_FIELD]

                    self.add_output(feature, meta)

                feature_with_type = "{}.{}".format(feature_type, mapping[IOTEA_FIELD][FEATURE_FIELD])
                self.provided_features[feature_with_type] = mapping

                if pubsub_topic not in self.dapr_subscriptions:
                    self.dapr_subscriptions[pubsub_topic] = list()

                self.dapr_subscriptions[pubsub_topic].append(mapping)

            dapr_out = sub_json[OUT_FIELD]

            for iotea_mapping in dapr_out:
                self.iotea_subscriptions[iotea_mapping[IOTEA_FIELD][FEATURE_FIELD]] = iotea_mapping
                if iotea_mapping[IOTEA_FIELD][INSTANCE_FIELD] == None:
                    self.subscription_rules.append(Rule(OpConstraint(
                        feature = iotea_mapping[IOTEA_FIELD][FEATURE_FIELD],
                        op = OpConstraint.OPS['ISSET'],
                        value = None,
                        type_selector = iotea_mapping[IOTEA_FIELD][TYPE_FILED],
                        value_type = Constraint.VALUE_TYPE[iotea_mapping[IOTEA_FIELD][VALUE_TYPE_FIELD]])))
                else:
                    self.subscription_rules.append(Rule(OpConstraint(
                        feature = iotea_mapping[IOTEA_FIELD][FEATURE_FIELD],
                        op = OpConstraint.OPS['ISSET'],
                        value = None,
                        type_selector = iotea_mapping[IOTEA_FIELD][TYPE_FILED],
                        value_type = Constraint.VALUE_TYPE[iotea_mapping[IOTEA_FIELD][VALUE_TYPE_FIELD]],
                        instance_id_filter = iotea_mapping[IOTEA_FIELD][INSTANCE_FIELD])))

        except:
            e = sys.exc_info()[0]
            self.logger.error("dapr pub-sub mappings could not be set ({})".format(e))

        self.logger.info("dapr pub-sub subscriptions for mapping {}".format(self.dapr_subscriptions))

    def __init_rpc_mappings(self):
        self.logger.debug("Initialize rpc mappings")

        try:
            sub_file = open("config/rpc_mapping.json")
            sub_json = json.load(sub_file)
            dapr_out = sub_json[OUT_FIELD]
            dapr_in = sub_json[IN_FIELD]

            # Create Mapping for outgoing requests to dapr
            for mapping in dapr_out:
                dapr_method = mapping[DAPR_FIELD][METHOD_NAME_FIELD]
                dapr_service_provider = mapping[DAPR_FIELD][SERVICE_PROVIDER_FIELD]
                iotea_method = mapping[IOTEA_FIELD][METHOD_NAME_FIELD]
                self.dapr_calles.append(iotea_method)
                self.logger.info("Register dapr function {}".format(dapr_method))
                self.register_function(iotea_method, partial(self.__handle_iotea_request, dapr_service_provider, dapr_method))
                self.dapr_rpc_bindings["{}.{}".format(dapr_service_provider, dapr_method)] = mapping[DAPR_FIELD]

            # Create Mapping for incoming requests from dapr
            for mapping in dapr_in:
                iotea_method = mapping[IOTEA_FIELD][METHOD_NAME_FIELD]
                iotea_type = mapping[IOTEA_FIELD][SERVICE_PROVIDER_FIELD]
                dapr_method = mapping[DAPR_FIELD][METHOD_NAME_FIELD]
                self.dapr_calles.append("{}.{}".format(iotea_type, iotea_method))
                self.logger.info("Register iotea function {}".format(iotea_method))
                self._servicer.register_method(dapr_method, partial(self.__handle_dapr_request, iotea_type, iotea_method))
                self.iotea_rpc_bindings["{}.{}".format(iotea_type, iotea_method)] = mapping[IOTEA_FIELD]

        except:
            e = sys.exc_info()[0]
            self.logger.error("dapr rpc mapping could not be set ({})".format(e))

        self.logger.info("Subscribed RPCs: {}".format(self.dapr_calles))

    def __dapr_rpc_mock(self, reg_data): # TODO: Remove mock
        text = "{{ \"value\": \"{} {}\" }}".format(reg_data["dapr_echo"], reg_data["dapr_name"])
        return InvokeServiceResponse(text, CONTENT_TYPE_JSON)

    # TODO It is not working as async function (future issues), but in examples such calls are async functions
    def __handle_iotea_request(self, service_provider, method, *args):
        self.logger.info("Request from iotea to dapr arrived")

        full_method = "{}.{}".format(service_provider, method)

        iotea_func_arguments = args[0:-2]
        self.logger.info("Received function for dapr method {} and args {}".format(full_method, iotea_func_arguments))

        #  Find rpc bindings
        if full_method not in self.dapr_rpc_bindings:
                self.logger.error("Method {} not in {}".format(full_method, self.dapr_rpc_bindings))
                return

        rpc_mapping = self.dapr_rpc_bindings[full_method]
        self.logger.debug("RPC MAPPING: {}".format(rpc_mapping))

        args_mapping = rpc_mapping[ARGS_BINDING]

        # Check that mappings equals argument list
        assert(len(iotea_func_arguments) == len(args_mapping))

        # Map arguments
        req_data = dict()
        for i in range(0, len(iotea_func_arguments)):
            try:
                req_data[args_mapping[i]] = iotea_func_arguments[i]
            except:
                self.logger.error("Function arguements for {} could not be mapped. Check mapping!".format(full_method))
                return "ERROR"

        self.logger.debug("Arguments mapped {}".format(req_data))

        if USE_DAPR_SERVICE_MOCK:
            # Call dapr mock for request
            response = self.__dapr_rpc_mock(req_data)
            self.logger.debug("Response from Dapr received: {} ({})".format(response.text(), response.content_type))
        else:
            response = self.dapr_client.invoke_service(
                id=service_provider,
                method=method,
                content_type=CONTENT_TYPE_JSON,
                data=json.dumps(req_data)
            )

        if TEST_IOTEA_RPC_INTERNAL:
            # Trigger iotea-echo request for testing
            data = json.dumps({
                    'iotea_echo': 'Hello',
                    'iotea_name': 'Dapr'
                    })
            content_type = CONTENT_TYPE_JSON
            invoke_request = InvokeServiceRequest(data, content_type)
            self.__handle_dapr_request("iotea-rpc_talent", "iotea-echo", invoke_request)

        if response.content_type is CONTENT_TYPE_JSON:
            result_json = json.loads(response.text())
            self.logger.debug("Result_json = {}".format(result_json))

            result = result_json[rpc_mapping[VALUE_REF_FIELD]]
            self.logger.debug("Result = {}".format(result))
            return result

        else:
            self.logger.error("Content-Type \' {} \' is not supported".format(response.content_type))

    def __handle_dapr_request(self, iotea_type, method, request: InvokeServiceRequest) -> InvokeServiceResponse:
        self.logger.info("Request from iotea to dapr arrived")

        if request.content_type is CONTENT_TYPE_JSON:
            dapr_func_arguments = json.loads(request.data)

            full_method = "{}.{}".format(iotea_type, method)

            if full_method not in self.iotea_rpc_bindings:
                self.logger.error("Method {} not in {}".format(full_method, self.iotea_rpc_bindings))
                return

            rpc_mapping = self.iotea_rpc_bindings[full_method]
            self.logger.debug("RPC MAPPING: {}".format(rpc_mapping))

            args_mapping = rpc_mapping[ARGS_BINDING]

            # Check that mappings equals argument list
            assert(len(dapr_func_arguments) == len(args_mapping))

            # Map arguments
            req_data = list()

            for i in range(0, len(dapr_func_arguments)):
                try:
                    req_data.append(dapr_func_arguments[args_mapping[i]])
                except:
                    self.logger.error("Function arguements for {} could not be mapped. Check mapping!".format(full_method))
                    return InvokeServiceResponse("{{ \"value\": \"{}\" }}".format("ERROR"), CONTENT_TYPE_JSON)

            self.logger.debug("Arguments mapped {}".format(req_data))

            self.logger.info("Call {}.{} with {}".format(iotea_type, method, req_data))
            coroutine = self.call(iotea_type, method,
                            req_data,
                            self.id,
                            INGESTION_TOPIC_WITH_PREFIX,
                            TIMEOUT_IOTEA_RPC_CALL)
            future = asyncio.run_coroutine_threadsafe(coroutine, self.loop)
            result = future.result(TIMEOUT_IOTEA_RPC_CALL)

            self.logger.debug("Result from {}.{} was {}".format(iotea_type, method, result))

        else:
            self.logger.error("Content-Type \' {} \' is not supported".format(request.content_type))


        response = InvokeServiceResponse("{{ \"value\": \"{}\" }}".format(result), CONTENT_TYPE_JSON)
        return response

    def __subscribe(self, pubsub_name: str, topic: str, metadata: Optional[Dict[str, str]] = {}):  # TODO: Private Method
        self.logger.info("Subsrcribe to dapr pubsub_name:{} topic:{}".format(pubsub_name,topic))
        self._servicer.register_topic(pubsub_name, topic, self.__on_dapr_event, metadata)

    def __run(self, app_port: Optional[int]) -> None:
        if app_port is None:
            self.logger.info("Use App port from dapr settings")
            app_port = settings.GRPC_APP_PORT

        self.logger.info("Start dapr thread for app_port:{}".format(app_port))
        self._server.add_insecure_port(f"[::]:{app_port}")
        self._server.start()

    async def __onPlatformEvent(self, ev, topic):
        # Use platform events to check if someone subscribes to topic which is via dapr (mapping available)
        if ev["type"] == PLATFORM_EVENT_TYPE_SET_RULES or ev["type"] == PLATFORM_EVENT_TYPE_UNSET_RULES:
            await self.__search_rule_tree(rules=ev["data"]["rules"],
                                          platform_type=ev["type"],
                                          talent=ev["data"]["talent"])

        if self.last_active_feature_subscriptions != self.active_feature_subscriptions:
            self.logger.info("Active subscriptions: {}".format(self.active_feature_subscriptions))

        self.last_active_feature_subscriptions = self.active_feature_subscriptions.copy()

    async def __search_rule_tree(self, rules, platform_type, talent):
        if "rules" in rules:
            rules = rules["rules"]
        else:
            self.logger.error("No key \"rules\" in dict")
            return

        for rule in rules:
            if "feature" in rule:
                #Subscribe / unsubscribe feature
                feature = rule[FEATURE_FIELD]

                if feature in self.provided_features:
                    # Set or increase feature subscription count
                    if platform_type == PLATFORM_EVENT_TYPE_SET_RULES:
                        if feature not in self.active_feature_subscriptions:
                            subscribers = list()
                            subscribers.append(talent)
                            self.active_feature_subscriptions[feature] = subscribers
                        else:
                            if talent not in self.active_feature_subscriptions[feature]:
                                self.active_feature_subscriptions[feature].append(talent)

                    # Remove or decrease feature subscription count
                    elif platform_type == PLATFORM_EVENT_TYPE_UNSET_RULES:
                        if feature in self.active_feature_subscriptions:
                            if talent in self.active_feature_subscriptions[feature]:
                                self.active_feature_subscriptions[feature].remove(talent)
                            if len(self.active_feature_subscriptions[feature]) <= 0:
                                del self.active_feature_subscriptions[feature]

            # Recursive call if rule contains ruleset
            if "rules" in rule:
                await self.__search_rule_tree(rule, platform_type, talent)

    async def __check_rpc(self, ev, evtctx):
        for func in self.dapr_calles:
            feature = "{}.{}-in".format(self.id, func)

            if ev['feature'] == feature:
                await super().on_event(ev, evtctx)
                return True

        # If no function subscription could be found and handled
        return False

    def __on_dapr_event(self, ev: v1.Event, meta) -> None:
        self.logger.debug("dapr event arrived:{}, data:{}, pubsub:{}, topic:{}".format(ev, ev.Data(), meta.pubsub_name, meta.topic))
        mappings = self.dapr_subscriptions["{}.{}".format(meta.pubsub_name, meta.topic)]

        for mapping in mappings:
            self.logger.debug("Mapping found {}".format(mapping))
            feature = mapping[IOTEA_FIELD][FEATURE_FIELD]
            feature_type = mapping[IOTEA_FIELD][TYPE_FILED]

            # If feature_type is none use self.id (defined by set_output)
            if feature_type == None:
               feature_type = self.id

            feature_with_type = "{}.{}".format(feature_type, feature)

            if feature_with_type in self.active_feature_subscriptions:
                millis = int(round(time.time() * 1000))
                data = json.loads(ev.Data().decode())  # Byte string to string
                value_key = mapping[DAPR_FIELD][VALUE_REF_FIELD]
                value = data[value_key]

                msg = {
                    "subject" : self.id,
                    "feature" : feature_with_type,
                    "value": value,
                    "whenMs" : millis }

                self.logger.debug("Publish to {} with payload {}".format(INGESTION_TOPIC, msg))

                try:
                    result = self.publish_mqtt_client.publish(
                        topic=INGESTION_TOPIC_WITH_PREFIX,
                        payload=json.dumps(msg))

                    if result.rc != mqtt.MQTT_ERR_SUCCESS:
                        self.logger.error("Message could not be published (rc={})".format(result.rc))

                except:
                    e = sys.exc_info()[0]
                    self.logger.error("Message could not be published ({})".format(e))

# Main
async def main():
    t = DaprAdapter(mqtt_host="localhost", mqtt_port=1883)
    await t.start(50051)

loop = asyncio.get_event_loop()
loop.run_until_complete(main())
loop.close()
