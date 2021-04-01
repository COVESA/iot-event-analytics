/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const { DEFAULT_TYPE, DEFAULT_INSTANCE } = require('../../../../src/core/constants');
const {
    TalentInput,
    TalentOutput,
    FeatureMetadata
} = require('../../../../src/core/util/talentIO');

describe('core.util.talentIO', () => {
    const ev = require('../../../resources/talentIO.event.json');

    describe('TalentInput', () => {
        it('should read the raw value', () => {
            expect(TalentInput.getRawValue(ev)).toBe(2);
        });

        it('should read all given raw values', () => {
            const allRawValues = TalentInput.getRawValue(ev, 100);

            expect(allRawValues.length).toBe(3);
        });

        it('should read all given raw values together with the timestamp', () => {
            const allRawValues = TalentInput.getRawValue(ev, 100, true);

            expect(allRawValues.length).toBe(3);

            for (let i = 0; i < allRawValues.length; i++) {
                expect(allRawValues[i].ts).toBeDefined();
                expect(allRawValues[i].value).toBeDefined();
            }
        });

        it('should read the encoded value', () => {
            expect(TalentInput.getEncodedValue(ev)).toBe(0.6666666666666666);
        });

        it ('should read all available instance ids', () => {
            expect(TalentInput.getInstancesFor(ev)).toEqual([ '4711' ]);
        });

        it ('should read the metadata of the given feature', () => {
            const metadata = TalentInput.getMetadata(ev);

            expect(metadata).toEqual(ev.$features.Vehicle['Body$Lights$IsBrakeOn'].$metadata);
        });

        it ('should read the statistical information of the given feature', () => {
            const stats = TalentInput.getStats(ev);

            expect(stats).toEqual(ev.$features.Vehicle['Body$Lights$IsBrakeOn']['4711'].$feature.stat);
        });

        it ('should evaluate given $vpath property automatically, if ignoreValuePath is set to false', () => {
            expect(TalentInput.getRawValue(ev, 10, false, 'foo', 'Vehicle', '4711', false)).toEqual([ 12, 11, 10 ]);
            expect(TalentInput.getRawValue(ev, 10, false, 'foo', 'Vehicle', '4711', true)).toEqual([ { $vpath: 'val', val: 12 }, { $vpath: 'val', val: 11 }, { $vpath: 'val', val: 10 } ]);
            expect(TalentInput.getRawValue(ev, 10, true, 'foo')).toEqual([ { ts: 1606820429584, value: 12 }, { ts: 1606820427604, value: 11 }, { ts: 1606820425626, value: 10 } ]);
        });
    });

    describe('FeatureMetadata', () => {
        it('should retrieve the unit of measurement for the given feature', () => {
            const unit = FeatureMetadata.getUnit(TalentInput.getMetadata(ev));

            expect(unit).toEqual(ev.$features.Vehicle['Body$Lights$IsBrakeOn'].$metadata.$unit);
        })
    });

    describe('TalentOutput', () => {
        it('should create an output feature for a given Talent', () => {
            const now = Date.now();
            const output = TalentOutput.create({ id: 'test' }, ev, 'myfeature', 3, ev.subject, DEFAULT_TYPE, DEFAULT_INSTANCE, now);

            expect(output).toEqual({
                subject: 'someuserid',
                type: DEFAULT_TYPE,
                instance: DEFAULT_INSTANCE,
                value: 3,
                feature: 'test.myfeature',
                whenMs: now
            });
        });

        it('should create an arbitary output', () => {
            const now = Date.now();
            const output = TalentOutput.createFor(ev.subject, DEFAULT_TYPE, DEFAULT_INSTANCE, 'anotherfeature', 22, now);

            expect(output).toEqual({
                subject: 'someuserid',
                type: DEFAULT_TYPE,
                instance: DEFAULT_INSTANCE,
                value: 22,
                feature: 'anotherfeature',
                whenMs: now
            });
        });

        it('should create multiple outputs as an array', () => {
            const now = Date.now();

            const to = new TalentOutput();
            to.add({id: 'test'}, ev, 'myfeature2', 1337, ev.subject, DEFAULT_TYPE, DEFAULT_INSTANCE, now);
            to.add({id: 'test'}, ev, 'myfeature3', 22, ev.subject, DEFAULT_TYPE, DEFAULT_INSTANCE, now);
            to.addFor('someuserid2', 'mytype', 'myinstance', 'myfeature4', 33, now);

            expect(to.toJson()).toEqual([
                {
                    subject: 'someuserid',
                    type: DEFAULT_TYPE,
                    instance: DEFAULT_INSTANCE,
                    value: 1337,
                    feature: 'test.myfeature2',
                    whenMs: now
                },
                {
                    subject: 'someuserid',
                    type: DEFAULT_TYPE,
                    instance: DEFAULT_INSTANCE,
                    value: 22,
                    feature: 'test.myfeature3',
                    whenMs: now
                },
                {
                    subject: 'someuserid2',
                    type: 'mytype',
                    instance: 'myinstance',
                    value: 33,
                    feature: 'myfeature4',
                    whenMs: now
                }
            ]);
        });
    });
});