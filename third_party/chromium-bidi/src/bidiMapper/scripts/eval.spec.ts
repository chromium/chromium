/**
 * Copyright 2021 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import fs from 'fs/promises';
import path from 'path';
import * as chai from 'chai';
import chaiExclude from 'chai-exclude';
import { CommonDataTypes } from '../bidiProtocolTypes';

chai.use(chaiExclude);

describe('Evaluator', function () {
    let EVALUATOR: {
        evaluate: Function,
        serialize: (x: any) => CommonDataTypes.RemoteValue,
        deserialize: (x: CommonDataTypes.RemoteValue) => any
    };

    // Get EVALUATOR.
    before(async function () {
        const eval_text = (await fs.readFile(
            path.join(__dirname, './eval.es'),
            'utf8'
        )).toString();

        EVALUATOR = eval(eval_text);
    });

    describe('serialize + deserialize', function () {
        function checkSerializeAndDeserialize(originalObject: any,
            expectedSerializedObj: CommonDataTypes.RemoteValue,
            excluding: string[] = []) {
            // Check serialize.
            const serializedOrigianlObj = EVALUATOR.serialize(originalObject);
            if (excluding.length > 0) {
                chai.assert.deepEqualExcludingEvery(
                    serializedOrigianlObj,
                    expectedSerializedObj,
                    excluding);

            } else {
                chai.assert.deepEqual(
                    serializedOrigianlObj,
                    expectedSerializedObj);
            }

            // Check deserialize.
            const deserializedSerializedOrigianlObj = EVALUATOR.deserialize(serializedOrigianlObj)
            chai.assert.strictEqual(
                deserializedSerializedOrigianlObj,
                originalObject);
        }

        describe('number', function () {
            it(`natural number`, function () {
                checkSerializeAndDeserialize(
                    42,
                    {
                        type: 'number',
                        value: 42
                    }
                );
            });
            it(`NaN`, function () {
                it(`serialize`, function () {
                    checkSerializeAndDeserialize(
                        NaN,
                        {
                            type: 'number',
                            value: 'NaN'
                        });
                });
            });
            it(`-0`, function () {
                it(`serialize`, function () {
                    checkSerializeAndDeserialize(
                        -0,
                        {
                            type: 'number',
                            value: '-0'
                        });
                });
            });
            it(`-Infinity`, function () {
                it(`serialize`, function () {
                    checkSerializeAndDeserialize(
                        -Infinity,
                        {
                            type: 'number',
                            value: '-Infinity'
                        });
                });
            });
            it(`+Infinity`, function () {
                it(`serialize`, function () {
                    checkSerializeAndDeserialize(
                        +Infinity,
                        {
                            type: 'number',
                            value: '+Infinity'
                        });
                });
            });
        });
        it('undefined', function () {
            checkSerializeAndDeserialize(
                undefined,
                {
                    type: 'undefined'
                }
            );
        });
        it('boolean', function () {
            checkSerializeAndDeserialize(
                false,
                {
                    type: 'boolean',
                    value: false
                }
            );
            checkSerializeAndDeserialize(
                true,
                {
                    type: 'boolean',
                    value: true
                }
            );
        });
        describe('string', function () {
            it('normal string', function () {
                checkSerializeAndDeserialize(
                    'SOME_STRING_HERE',
                    {
                        type: 'string',
                        value: 'SOME_STRING_HERE'
                    }
                );
            });
            it('empty string', function () {
                checkSerializeAndDeserialize(
                    '',
                    {
                        type: 'string',
                        value: ''
                    }
                );
            });
        });
        describe('object', function () {
            it('flat object', function () {
                checkSerializeAndDeserialize(
                    {
                        SOME_PROPERTY: 'SOME_VALUE'
                    },
                    {
                        type: 'object',
                        value: [[
                            "SOME_PROPERTY",
                            {
                                type: "string",
                                value: "SOME_VALUE"
                            }]],
                        objectId: '__any_value__'
                    },
                    ["objectId"]
                );
            });
            it('nested objects', function () {
                checkSerializeAndDeserialize(
                    {
                        'foo': {
                            'bar': 'baz'
                        },
                        'qux': 'quux'
                    },
                    {
                        "type": "object",
                        "objectId": "__any_value__",
                        "value": [[
                            "foo", {
                                "type": "object",
                                "objectId": "__any_value__"
                            }], [
                            "qux", {
                                "type": "string",
                                "value": "quux"
                            }]]
                    },
                    ["objectId"]
                );
            });
        });
        it('function', function () {
            checkSerializeAndDeserialize(
                function () { },
                {
                    type: 'function',
                    objectId: '__any_value__'
                },
                ["objectId"]
            );
        });
        it('array', function () {
            checkSerializeAndDeserialize(
                [1, 'a', { foo: 'bar' }, [2, [3, 4]]],
                {
                    type: "array",
                    objectId: "__any_value__",
                    value: [{
                        type: "number",
                        value: 1
                    }, {
                        type: "string",
                        value: "a"
                    }, {
                        type: "object",
                        objectId: "__any_value__"
                    }, {
                        type: "array",
                        objectId: "__any_value__"
                    }]
                }, ["objectId"]
            );
        });
        it('promise', function () {
            checkSerializeAndDeserialize(
                Promise.resolve(),
                {
                    type: 'promise',
                    objectId: '__any_value__'
                },
                ["objectId"]
            );
        });
    });
});
