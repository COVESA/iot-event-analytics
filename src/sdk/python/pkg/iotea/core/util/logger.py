##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import logging
import json
import uuid
import sys

class Context:
    def __init__(self, data=None, mapped_format=None):
        self.data = None
        self.mapped_format = None
        self.refresh(data, mapped_format)

    def clear(self):
        self.data = None

    # mapped_format = "{foo} is {bar}"  with data = { "foo": 1, "bar": "tralala" }
    def refresh(self, data=None, mapped_format=None):
        self.data = data

        if mapped_format is not None:
            self.mapped_format = mapped_format

    def __str__(self):
        if self.data is None:
            return ''

        if isinstance(self.data, str):
            return self.data

        if self.mapped_format is None:
            return json.dumps(self.data)

        return self.mapped_format.format_map(self.data)

class LogFormatHandler(logging.StreamHandler):
    def __init__(self, stream=sys.stdout):
        super(LogFormatHandler, self).__init__(stream)
        self.setFormatter(logging.Formatter('', ''))
        # Set ISO 8601 as default
        self.reset()

    def reset(self):
        # Set ISO 8601 as default
        self.setFormatter(logging.Formatter('%(asctime)s.%(msecs)03dZ %(levelname)s [ %(displayname)s ]: %(message)s%(context)s', '%Y-%m-%dT%H:%M:%S'))

class Logger(logging.Logger):
    def __init__(self, name):
        super().__init__(str(uuid.uuid4()))
        self.display_name = name
        self.reset()

    def reset(self):
        self.handlers = [LogFormatHandler()]
        # Do not set any level here. It is set by the RootLogger
        self.setLevel(logging.NOTSET)

    # Method, which is called by everyone
    def _log(self, level, msg, args, exc_info=None, extra=None, stack_info=False):
        # Ensure extra is a dictionary, if not, create one
        if not isinstance(extra, dict):
            extra = {}

        extra['displayname'] = self.display_name

        # Ensure context field in extra is not a string
        extra['context'] = str(extra['context']) if 'context' in extra else ''

        super(Logger, self)._log(level, msg, args, exc_info, extra)

    def create_extra(self, ctx):
        return {
            'displayname': self.display_name,
            'context': ctx
        }

    @staticmethod
    def create_context(data, mapped_format):
        return Context(data, mapped_format)

    @staticmethod
    def create_event_context(ev=None):
        if ev is None:
            ev = {}

        return Context({'cid': ev['cid'] if 'cid' in ev else uuid.uuid4()}, ' {cid}')

    @staticmethod
    def resolve_log_level(level_string):
        try:
            return int(getattr(
                logging,
                str(level_string),
                None
            ))
        except:
            raise Exception('Received invalid loglevel string {}'.format(level_string))
