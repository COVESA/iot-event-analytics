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
import logging

from iotea.core.protocol_gateway import ProtocolGateway
from iotea.core.util.logger import Logger
from iotea.core.util.mqtt_client import MqttProtocolAdapter

logging.setLoggerClass(Logger)

# pylint: disable=wrong-import-position
from iotea.core.talent import Talent
from iotea.core.rules import AndRules, Rule, OpConstraint, Constraint
from iotea.core.constants import DEFAULT_TYPE

MAX_CHARGING_POWER_KW = 41
MAX_TRANSACTION_STEP_DELAY = 8000
COST_PER_KHW_CENTS = 19

logging.setLoggerClass(Logger)
logging.getLogger().setLevel(logging.INFO)

class ChargingStationTalent(Talent):
    def __init__(self, protocol_gateway):
        super().__init__('ChargingStationTalent', protocol_gateway)

    def get_rules(self):
        return AndRules([
            Rule(OpConstraint('ChargeAPI.chargeOffer', OpConstraint.OPS['ISSET'], None, DEFAULT_TYPE, Constraint.VALUE_TYPE['RAW']))
        ])

    def callees(self):
        return [
            'ChargeAPI.getVIN',
            'ChargeAPI.getCredential',
            'ChargeAPI.postOfferAccept',
            'ChargeAPI.postTransactionEnd',
            'VerifyTalent.verify'
        ]

    # pylint: disable=unused-argument
    async def verify(self, data):
        self.logger.warning('Verify not implemented')
        return 'Verified'

    # pylint: disable=unused-argument
    async def charge_transaction(self, ev, evtctx):
        session_id = ev['value']['session']
        # Get Vehicle Credential with allowed KW
        self.logger.info('Call GetCredential ')

        credential = await self.call(
            'ChargeAPI',
            'getCredential',
            [
                session_id
            ],
            ev['subject'],
            ev['returnTopic'],
            2000
        )

        self.logger.info('GetCredential result {}'.format(credential))

        self.logger.info('Call Verify {}'.format(credential))

        verification_result = await self.call(
            'VerifyTalent',
            'verify',
            [
                credential
            ],
            ev['subject'],
            ev['returnTopic'],
            10000
        )

        self.logger.info('Verification result {}'.format(verification_result))
        certified_credential = json.loads(verification_result)

        self.logger.info('Certified credential {}'.format(certified_credential))

        max_charging = certified_credential['credentialSubject']['vehicle']['maxcharging']['rate']
        vin = certified_credential['credentialSubject']['vehicle']['vin']

        charge_features = {
            'maxChargePowerKW' : max_charging,
            'centsPerKWh' : COST_PER_KHW_CENTS
        }

        self.logger.info('Call postOfferAccept with {}'.format(charge_features))

        result = await self.call(
            'ChargeAPI',
            'postOfferAccept',
            [
                session_id, charge_features
            ],
            ev['subject'],
            ev['returnTopic'],
            MAX_TRANSACTION_STEP_DELAY
        )

        if result is not True:
            self.logger.info('Offer was not accepted')
            return

        self.logger.info('Call postTransactionEnd for Vin {}'.format(vin))

        result = await self.call('ChargeAPI', 'postTransactionEnd', [session_id, True], ev['subject'], ev['returnTopic'], MAX_TRANSACTION_STEP_DELAY)

        self.logger.info('Result: {}'.format(result))

    async def on_event(self, ev, evtctx):
        self.logger.info('Event arrived...')

        try:
            await self.charge_transaction(ev, evtctx)
        # pylint: disable=broad-except
        except Exception as err:
            self.logger.warning('Transaction failed. Error: {}'.format(err))

async def main():
    mqtt_config = MqttProtocolAdapter.create_default_configuration()
    pg_config = ProtocolGateway.create_default_configuration([mqtt_config])

    charging_station_talent = ChargingStationTalent(pg_config)
    await charging_station_talent.start()

LOOP = asyncio.get_event_loop()
LOOP.run_until_complete(main())
LOOP.close()
