##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import grpc

from cloudevents.sdk.event import v1
from concurrent import futures
from google.protobuf import empty_pb2

from typing import Callable

from dapr.ext.grpc._servicier import _CallbackServicer
from dapr.proto import appcallback_service_v1

TopicSubscribeCallable = Callable[[v1.Event], None]

class Meta():
    def __init__(self):
        self.pubsub_name = None
        self.topic = None

class DaprGRPC(_CallbackServicer):
    def __init__(self):
        super(DaprGRPC, self).__init__()

    def create_server(self, **kwargs):
        server = None
        if not kwargs:
            server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
        else:
            server = grpc.server(**kwargs)

        # Bind servicer to server
        appcallback_service_v1.add_AppCallbackServicer_to_server(self, server)

        return server

    # Override super
    def OnTopicEvent(self, request, context):
        pubsub_topic = request.pubsub_name + ":" + request.topic
        if pubsub_topic not in self._topic_map:
            context.set_code(grpc.StatusCode.UNIMPLEMENTED)
            raise NotImplementedError(f'topic {request.topic} is not implemented!')

        event = v1.Event()
        event.SetEventType(request.type)
        event.SetEventID(request.id)
        event.SetSource(request.source)
        event.SetData(request.data)
        event.SetContentType(request.data_content_type)

        meta = Meta()
        meta.pubsub_name = request.pubsub_name
        meta.topic = request.topic

        self._topic_map[pubsub_topic](event, meta)

        return empty_pb2.Empty()