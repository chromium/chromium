/**
 * Copyright 2022 Google LLC.
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

import * as chai from 'chai';
import { CommonDataTypes } from '../protocol/bidiProtocolTypes';
import { logMessageFormatter, getRemoteValuesText } from './logHelper';

const expect = chai.expect;

describe('test getRemoteValuesText', function () {
  it('single line input test', async function () {
    const inputArgs = [{ type: 'string', value: 'line 1' }];
    const outputString = 'line 1';
    expect(
      getRemoteValuesText(inputArgs as CommonDataTypes.RemoteValue[], false)
    ).to.equal(outputString);
  });

  it('multiple line input test', async function () {
    const inputArgs = [
      { type: 'string', value: 'line 1' },
      { type: 'string', value: 'line 2' },
    ];
    const outputString = 'line 1\u0020line 2';
    expect(
      getRemoteValuesText(inputArgs as CommonDataTypes.RemoteValue[], false)
    ).to.equal(outputString);
  });

  it('no input test', async function () {
    const inputArgs = [] as any[];
    const outputString = '';
    expect(
      getRemoteValuesText(inputArgs as CommonDataTypes.RemoteValue[], false)
    ).to.equal(outputString);
  });
});

describe('test logMessageFormatter', function () {
  it('integer input test', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %d string test' },
      { type: 'number', value: 1 },
    ];
    const outputString = 'test string 1 string test';
    expect(
      logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
    ).to.equal(outputString);
  });

  it('float input test', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %f string test' },
      { type: 'number', value: 1.23 },
    ];
    const outputString = 'test string 1.23 string test';
    expect(
      logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
    ).to.equal(outputString);
  });

  it('string input test', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %s string test' },
      { type: 'string', value: 'test string' },
    ];
    const outputString = 'test string test string string test';
    expect(
      logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
    ).to.equal(outputString);
  });

  it('object input test', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %o string test' },
      { type: 'object', value: [['id', { type: 'number', value: 1 }]] },
    ];
    const outputString = 'test string {"id": 1} string test';
    expect(
      logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
    ).to.equal(outputString);
  });

  it('object input test 2', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %O string test' },
      { type: 'object', value: [['id', { type: 'number', value: 1 }]] },
    ];
    const outputString = 'test string {"id": 1} string test';
    expect(
      logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
    ).to.equal(outputString);
  });

  it('CSS input test', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %c string test' },
      {
        type: 'object',
        value: [['font-size', { type: 'string', value: '20px' }]],
      },
    ];
    const outputString = 'test string {"font-size": "20px"} string test';
    expect(
      logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
    ).to.equal(outputString);
  });

  it('more values', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %i string test' },
      { type: 'number', value: 1 },
      { type: 'number', value: 2 },
    ];
    const outputString =
      'More value is provided: "test string %i string test 1 2"';

    try {
      expect(
        logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
      ).to.equal(outputString);
    } catch (e) {
      expect((e as Error).message).to.eq(outputString);
    }
  });

  it('less values', async function () {
    const inputArgs = [
      { type: 'string', value: 'test string %i %i string test' },
      { type: 'number', value: 1 },
    ];
    const outputString =
      'Less value is provided: "test string %i %i string test 1"';

    try {
      expect(
        logMessageFormatter(inputArgs as CommonDataTypes.RemoteValue[])
      ).to.equal(outputString);
    } catch (e) {
      expect((e as Error).message).to.eq(outputString);
    }
  });
});
