/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/
 
const {
    ENCODING_ENCODER_CATEGORY,
    ENCODING_ENCODER_MINMAX,
    ENCODING_ENCODER_DELTA,
    ENCODING_ENCODER_THROUGH,
    ENCODING_TYPE_NUMBER,
    ENCODING_TYPE_STRING,
    ENCODING_TYPE_OBJECT,
    ENCODING_TYPE_BOOLEAN,
    DEFAULT_FEATURE_TTL_MS
} = require('../../../src/core/constants');

const jsonata = require('jsonata');
const ProtocolGateway = require('../../../src/core/protocolGateway');
const Encoding = require('../../../src/core/encoding');
const NullLogger = require('../../helpers/null.logger');
const Instance = require('../../../src/core/instance');
const { MqttProtocolAdapter } = require('../../../src/core/util/mqttClient');


describe('core.encoding', () => {

    function createInstance(type, instanceId) {
        return new Instance(type, instanceId, new NullLogger());
    }

    function createEncoding() {
        return new Encoding(ProtocolGateway.createDefaultConfiguration([ MqttProtocolAdapter.createDefaultConfiguration(true, '')]));
    }

    const nowMs = Date(); 
    const i = createInstance('test', '4711');
    i.updateFeatureAt(1, null, 'test', nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);
    const enc = createEncoding();

    function createMetaFeature(encoder, type = '', min = 0, max = 0, range = null) {
        return  {
            idx: 1,
            description: '1',
            encoding: {
                encoder: encoder, // e.g. ENCODING_ENCODER_THROUGH,
                type: type,       // e.g. 'number'
                min : min,
                max : max,
                enum : range      // e.g. [4,5,6]
            }
        }     
    }

    function createReduceMetaFeature(encoder, type = '', reduce = '', range = '') {
        return  {
            idx: 1,
            description: '1',
            encoding: {
                encoder: encoder,  // e.g. ENCODING_ENCODER_THROUGH,
                reduce: reduce,    // 
                type: type,        // e.g. 'number'
                enum : range      // e.g. [4,5,6]
            }
        }     
    }



    describe('Start Encoder ', () => {
        it('should not be null ', () => {
            expect(enc.start()).not.toBeNull();;
        });
    });
    
    describe('__encodeValue encoder: ENCODING_ENCODER_THROUGH ', () => {
        it('should return a value (happy path): ', () => {

            const metaFeature = createMetaFeature(ENCODING_ENCODER_THROUGH, 'number');

            expect(enc.__encodeValue(0 , metaFeature, i.getFeatureAt(1))).toBe(0);
            expect(enc.__encodeValue(1 , metaFeature, i.getFeatureAt(1))).toBe(1);
            expect(enc.__encodeValue(.5 , metaFeature, i.getFeatureAt(1))).toBe(0.5);
        });

        it('should throw error because number out of range ', () => {

            expect(() => enc.__encodeValue(2 , 
                                        createMetaFeature(ENCODING_ENCODER_THROUGH, 'number'), 
                                        i.getFeatureAt(1))).toThrowError('Value 2 not within range [0..1]');
        });

        it('should throw error because wrong value type ', () => {

            expect(() => enc.__encodeValue('foo' , 
                                        createMetaFeature(ENCODING_ENCODER_THROUGH, 'number'), 
                                        i.getFeatureAt(1))).toThrowError('Expected number value, received string');
        });
    });

    describe('__encodeValue with encoder: ENCODING_ENCODER_MINMAX ', () => {
        it('should return a value (happy path): ', () => {

            const metaFeature = createMetaFeature(ENCODING_ENCODER_MINMAX, 'number', 1, 6);

            expect(enc.__encodeValue(1 , metaFeature, i.getFeatureAt(1))).toBe(0);
            expect(enc.__encodeValue(3 , metaFeature, i.getFeatureAt(1))).toBe(.4);
            expect(enc.__encodeValue(6 , metaFeature, i.getFeatureAt(1))).toBe(1);
            });

        it('should throw error because values out of range ', () => {

            const metaFeature = createMetaFeature(ENCODING_ENCODER_MINMAX, 'number', 1, 6);

            expect(() => enc.__encodeValue(0 , metaFeature, i.getFeatureAt(1))).toThrowError('Value 0 not within range [1..6]');
            expect(() =>  enc.__encodeValue(7 , metaFeature, i.getFeatureAt(1))).toThrowError('Value 7 not within range [1..6]');
        });

        it('should throw error because wrong min max ', () => {

            expect(() =>  enc.__encodeValue(3 , 
                                createMetaFeature(ENCODING_ENCODER_MINMAX, 'number', 6, 6), 
                                i.getFeatureAt(1))).toThrowError('Minimum and maximum need to define a range and must not be equal.');
        });
    });

    describe('__encodeValue with encoder: ENCODING_ENCODER_DELTA ', () => {
        it('should return a value (happy path): ', () => {

            expect(enc.__encodeValue(5 , createMetaFeature(ENCODING_ENCODER_DELTA, 'number', 1, 5), null)).toBe(0);

            const featureRaw = 4;
             i.updateFeatureAt(2, null, featureRaw, nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);  // feature.raw = 4  -> value = 6 - 4 = 2 -> input 2 for range 1 .. 5
            expect(enc.__encodeValue(6 , createMetaFeature(ENCODING_ENCODER_DELTA, 'number', 1, 5), i.getFeatureAt(2))).toBe(.25);
        });

        it('should throw error because wrong object type ', () => {

            expect(() => enc.__encodeValue('foo' , 
                                    createMetaFeature(ENCODING_ENCODER_DELTA, 'number', 1, 5), 
                                    i.getFeatureAt(1))).toThrowError('Expected number value, received string');
        });

        it('should throw error because value out of range ', () => {

            const featureRaw = 4;
            i.updateFeatureAt(2, null, featureRaw, nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);  
            expect(() => enc.__encodeValue(4 , 
                                    createMetaFeature(ENCODING_ENCODER_DELTA, 'number', 1, 5), 
                                    i.getFeatureAt(2))).toThrowError('Value 0 not within range [1..5]');  
        });
    });

    describe('__encodeValue with encoder: ENCODING_ENCODER_CATEGORY ', () => {

        it('should throw error because type == foo does not exist ' , () => {

            expect(() => enc.__encodeValue( 2 , 
                                            createMetaFeature(ENCODING_ENCODER_CATEGORY, 'foo' ), 
                                            i.getFeatureAt(1))).toThrowError('No categorical encoder found for given value type foo');  
        });

        describe('ENCODING_TYPE_BOOLEAN ', () => {
            it('should return a value (happy path): ', () => {

            expect(enc.__encodeValue(true , 
                                     createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_BOOLEAN, 0, 0, [true,false] ), 
                                     i.getFeatureAt(1))).toBe(0);  // returns _boolean[index]  ->  [true,false];
            });

            it('should throw error because wrong input type ', () => {

                expect(() => enc.__encodeValue('maybe' , 
                                            createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_BOOLEAN, 0, 0, [true,false] ), 
                                            i.getFeatureAt(1))).toThrowError('Expected boolean value, received string');
            });

            it('should throw error because too many entries in enum ', () => {

                expect(() => enc.__encodeValue(true , 
                                        createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_BOOLEAN, 0, 0, [true,false, 'maybe'] ), 
                                        i.getFeatureAt(1))).toThrowError('Enumerations for booleans only allow true or false');
            });
        });

        describe('ENCODING_TYPE_NUMBER ', () => {
            it('should return a value (happy path): ', () => {

                const featureRaw = 4;
                i.updateFeatureAt(4, null, featureRaw, nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);  
                expect(enc.__encodeValue(4 , 
                                    createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_NUMBER, 0, 0, [4,5,6] ), 
                                    i.getFeatureAt(4))).toBe(0);
            });

            it('should throw error because wrong input type ', () => {

                expect(() => enc.__encodeValue(true , 
                                    createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_NUMBER, 0, 0, [4,5,6] ), 
                                    i.getFeatureAt(1))).toThrowError('Expected number or string value, received boolean');
            });

            it('should throw error because value outside range ', () => {

                const featureRaw = 4;
                i.updateFeatureAt(3, null, featureRaw , nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);  
                expect(() => enc.__encodeValue(2 , 
                                    createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_NUMBER, 0, 0, [4,5,6] ), 
                                    i.getFeatureAt(3))).toThrowError('Value 2 was not found in given enumeration 4,5,6');
            });
        });

        describe('ENCODING_TYPE_STRING ', () => {
            it('should return a value (happy path): ', () => {

                const metaFeature = createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_STRING, 0, 0, ['4711','0711'] );

                const featureRaw = '4711';
                i.updateFeatureAt(8, null, featureRaw, nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs);  
                expect(enc.__encodeValue('4711' , metaFeature,  i.getFeatureAt(8))).toBe(0);

                const _pathobj = { value: '4711',
                                inQuestion: '0711',
                                $vpath: 'inQuestion'};
                const _refobj = { value: '4711',
                                inQuestion: 1,
                                $vpath: 'inQuestion'};

                expect(enc.__encodeValue(_pathobj , metaFeature, i.getFeatureAt(1))).toEqual(_refobj);   
            });

            it('should throw error because value not in enum ', () => {

                const featureRaw = 'Paul';
                i.updateFeatureAt(7, null, featureRaw, nowMs, 0, DEFAULT_FEATURE_TTL_MS, nowMs); 
                expect(() => enc.__encodeValue('Paul' , 
                                    createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_STRING, 0, 0, ['4711','0711'] ), 
                                    i.getFeatureAt(7))).toThrowError('Value Paul was not found in given enumeration 4711,0711');
            });
        });


        describe('ENCODING_TYPE_OBJECT ', () => {
            it('should return a value (happy path): ', () => {

                const _obj = ['4711'];
                expect(enc.__encodeValue(_obj , 
                                createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_OBJECT, 0, 0, [['0711'],['4711']] ), 
                                i.getFeatureAt(1))).toBe(1);

                const expression = `{
                    'part2': '4711' 
                }`;
                const range = [{'part1' : '0711'},{'part2' : '4711'}];

                expect(enc.__encodeValue('0711' , 
                                createReduceMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_OBJECT, expression, range ), 
                                i.getFeatureAt(1))).toBe(1); 
            });

            it('should throw error because wrong input type ', () => {

                expect(() => enc.__encodeValue(true , 
                                    createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_OBJECT, 0, 0, [['0711'],['4711']] ), 
                                    i.getFeatureAt(1))).toThrowError('Expected object value, received boolean');
            });


            it('should throw error because value not in enum ', () => {

                const _badobj = ['4712'];
                expect(() => enc.__encodeValue(_badobj , 
                                    createMetaFeature(ENCODING_ENCODER_CATEGORY, ENCODING_TYPE_OBJECT, 0, 0, [['0711'],['4711']] ), 
                                    i.getFeatureAt(1))).toThrowError('Value 4712 was not found in given enumeration objects [["0711"],["4711"]]');
            });

            it('should throw error because no enum in metaFeature ', () => {

                // Force enum to be undefined - 'null' would not force this error:
                const expression = `{
                    'part2': '4711' 
                }`

                const no_enum_metaFeature = {
                    idx: 1,
                    description: '1',
                    encoding: {
                        encoder: ENCODING_ENCODER_CATEGORY, 
                        reduce: expression,
                        type: ENCODING_TYPE_OBJECT
                    }
                }     
                expect(() => enc.__encodeValue(2 , no_enum_metaFeature, i.getFeatureAt(1))).toThrowError('Cannot read property \'entries\' of undefined'); 
            });
        });

    });

    describe('__encodeValue faulty encoder ', () => {
        it('should return null because encoder == null ', () => {

            const value = 'test';
            expect(enc.__encodeValue(value, createMetaFeature(null, 'number') , i.getFeatureAt(1))).toBe(null);
        });

        it('should throw error because encoder == foo does not exist ' , () => {

            expect(() => enc.__encodeValue(2 , createMetaFeature('foo', 'number' ), i.getFeatureAt(1))).toThrowError('Encoder foo not found');
        });

        it('should throw error because encoder == foo with wrong vpath' , () => {

            const _wrongobj = { value: '4711',
                                $vpath: 'Paul'};
            expect(() => enc.__encodeValue(_wrongobj , createMetaFeature('foo', 'number' ), i.getFeatureAt(1))).toThrowError('Invalid $vpath Paul');
        });        

        it('should throw error because encoder == foo with no path' , () => {

            expect(() => enc.__encodeValue(2 , 
                                    createReduceMetaFeature('foo', 'number', true), 
                                    i.getFeatureAt(1))).toThrowError('path.charAt is not a function');  
        });                
    });
});