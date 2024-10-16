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

import {expect} from 'chai';

import type {Script} from '../../../protocol/protocol.js';

import {getRemoteValuesText, logMessageFormatter} from './logHelper.js';

const STRING_FORMAT_TEST_CASES = [
  {
    name: 'undefined',
    arg: {type: 'undefined'},
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'undefined',
      o: 'undefined',
      O: 'undefined',
      c: 'undefined',
    },
  },
  {
    name: 'null',
    arg: {type: 'null'},
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'null',
      o: 'null',
      O: 'null',
      c: 'null',
    },
  },
  {
    name: '"STRING "\' ARGUMENT"',
    arg: {type: 'string', value: 'STRING ARGUMENT'},
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'STRING ARGUMENT',
      o: '"STRING ARGUMENT"',
      O: '"STRING ARGUMENT"',
      c: '"STRING ARGUMENT"',
    },
  },
  {
    name: '"42"',
    arg: {type: 'string', value: '42'},
    expected: {
      d: '42',
      f: '42',
      s: '42',
      o: '"42"',
      O: '"42"',
      c: '"42"',
    },
  },
  {
    name: '42',
    arg: {type: 'number', value: 42},
    expected: {
      d: '42',
      f: '42',
      s: '42',
      o: '42',
      O: '42',
      c: '42',
    },
  },
  {
    name: 'NaN',
    arg: {type: 'number', value: 'NaN'},
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'NaN',
      o: 'NaN',
      O: 'NaN',
      c: 'NaN',
    },
  },
  {
    name: '-0',
    arg: {type: 'number', value: '-0'},
    expected: {
      d: '0',
      f: '0',
      s: '-0',
      o: '-0',
      O: '-0',
      c: '-0',
    },
  },
  {
    name: 'Infinity',
    arg: {type: 'number', value: 'Infinity'},
    expected: {
      d: 'NaN',
      f: 'Infinity',
      s: 'Infinity',
      o: 'Infinity',
      O: 'Infinity',
      c: 'Infinity',
    },
  },
  {
    name: '-Infinity',
    arg: {type: 'number', value: '-Infinity'},
    expected: {
      d: 'NaN',
      f: '-Infinity',
      s: '-Infinity',
      o: '-Infinity',
      O: '-Infinity',
      c: '-Infinity',
    },
  },
  {
    name: '1.234',
    arg: {type: 'number', value: 1.234},
    expected: {
      d: '1',
      f: '1.234',
      s: '1.234',
      o: '1.234',
      O: '1.234',
      c: '1.234',
    },
  },
  {
    name: 'true',
    arg: {type: 'boolean', value: true},
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'true',
      o: 'true',
      O: 'true',
      c: 'true',
    },
  },
  {
    name: 'false',
    arg: {type: 'boolean', value: false},
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'false',
      o: 'false',
      O: 'false',
      c: 'false',
    },
  },
  {
    name: '1234567890n',
    arg: {type: 'bigint', value: '1234567890'},
    expected: {
      d: '1234567890',
      f: '1234567890',
      s: '1234567890',
      o: '1234567890n',
      O: '1234567890n',
      c: '1234567890n',
    },
  },
  {
    name: 'Array',
    arg: {
      type: 'array',
      value: [{type: 'undefined'}, {type: 'string', value: 'STRING ARGUMENT'}],
    },
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'Array(2)',
      o: '[undefined,"STRING ARGUMENT"]',
      O: '[undefined,"STRING ARGUMENT"]',
      c: '[undefined,"STRING ARGUMENT"]',
    },
  },
  {
    name: 'Date',
    arg: {
      type: 'date',
      value: '2020-07-19T07:34:56.789+01:00',
    },
    expected: {
      d: 'NaN',
      f: 'NaN',
      // `Date.toString` has timezone-dependent part, so it has to be
      // calculated each time.
      s: new Date('2020-07-19T07:34:56.789+01:00').toString(),
      o: '"2020-07-19T07:34:56.789+01:00"',
      O: '"2020-07-19T07:34:56.789+01:00"',
      c: '"2020-07-19T07:34:56.789+01:00"',
    },
  },
  {
    name: 'Map',
    arg: {
      type: 'map',
      value: [
        ['nameWithoutDashes', {type: 'number', value: 42}],
        ['NAME-WITH-DASH', {type: 'string', value: 'STRING ARGUMENT'}],
      ],
    },
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'Map(2)',
      o: 'Map(2)',
      O: 'Map(2)',
      c: 'Map(2)',
    },
  },
  {
    name: 'Object',
    arg: {
      type: 'object',
      value: [
        ['nameWithoutDashes', {type: 'number', value: 42}],
        ['NAME-WITH-DASH', {type: 'string', value: 'STRING ARGUMENT'}],
      ],
    },
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'Object(2)',
      o: '{"nameWithoutDashes":42,"NAME-WITH-DASH":"STRING ARGUMENT"}',
      O: '{"nameWithoutDashes":42,"NAME-WITH-DASH":"STRING ARGUMENT"}',
      c: '{"nameWithoutDashes":42,"NAME-WITH-DASH":"STRING ARGUMENT"}',
    },
  },
  {
    name: '/abc?/g',
    arg: {
      type: 'regexp',
      value: {pattern: 'abc?', flags: 'g'},
    },
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: '/abc?/g',
      o: '/abc?/g',
      O: '/abc?/g',
      c: '/abc?/g',
    },
  },
  {
    name: 'Set(undefined, "STRING_ARGUMENT")',
    arg: {
      type: 'set',
      value: [{type: 'undefined'}, {type: 'string', value: 'STRING ARGUMENT'}],
    },
    expected: {
      d: 'NaN',
      f: 'NaN',
      s: 'Set(2)',
      o: 'Set(2)',
      O: 'Set(2)',
      c: 'Set(2)',
    },
  },
];

function testPattern(
  formatString: string,
  argument: unknown,
  expected: string,
) {
  const inputArgs = [
    {
      type: 'string',
      value: formatString,
    },
    argument as Script.RemoteValue,
  ] satisfies Script.RemoteValue[];
  const result = logMessageFormatter(inputArgs);
  expect(result).to.equal(expected);
}

describe('logHelper', () => {
  describe('getRemoteValuesText', () => {
    it('single line input test', () => {
      const inputArgs = [
        {type: 'string', value: 'line 1'},
      ] satisfies Script.RemoteValue[];
      const outputString = 'line 1';
      expect(getRemoteValuesText(inputArgs, false)).to.equal(outputString);
    });

    it('multiple line input test', () => {
      const inputArgs = [
        {type: 'string', value: 'line 1'},
        {type: 'string', value: 'line 2'},
      ] satisfies Script.RemoteValue[];
      const outputString = 'line 1\u0020line 2';
      expect(getRemoteValuesText(inputArgs, false)).to.equal(outputString);
    });

    it('no input test', () => {
      const inputArgs = [] satisfies Script.RemoteValue[];
      const outputString = '';
      expect(getRemoteValuesText(inputArgs, false)).to.equal(outputString);
    });
  });

  describe('logMessageFormatter', () => {
    describe('respect %d argument', () => {
      const FORMAT_STRING = '%d';
      for (const testcase of STRING_FORMAT_TEST_CASES) {
        it(`${testcase.name} value`, () => {
          testPattern(FORMAT_STRING, testcase.arg, testcase.expected.d);
        });
      }
    });

    describe('respect %f argument', () => {
      const FORMAT_STRING = '%f';
      for (const testcase of STRING_FORMAT_TEST_CASES) {
        it(`${testcase.name} value`, () => {
          testPattern(FORMAT_STRING, testcase.arg, testcase.expected.f);
        });
      }
    });

    describe('respect %s argument', () => {
      const FORMAT_STRING = '%s';
      for (const testcase of STRING_FORMAT_TEST_CASES) {
        it(`${testcase.name} value`, () => {
          testPattern(FORMAT_STRING, testcase.arg, testcase.expected.s);
        });
      }
    });

    describe('respect %o argument', () => {
      const FORMAT_STRING = '%o';
      for (const testcase of STRING_FORMAT_TEST_CASES) {
        it(`${testcase.name} value`, () => {
          testPattern(FORMAT_STRING, testcase.arg, testcase.expected.o);
        });
      }
    });

    describe('respect %O argument', () => {
      const FORMAT_STRING = '%O';
      for (const testcase of STRING_FORMAT_TEST_CASES) {
        it(`${testcase.name} value`, () => {
          testPattern(FORMAT_STRING, testcase.arg, testcase.expected.O);
        });
      }
    });

    describe('respect %c argument', () => {
      const FORMAT_STRING = '%c';
      for (const testcase of STRING_FORMAT_TEST_CASES) {
        it(`${testcase.name} value`, () => {
          testPattern(FORMAT_STRING, testcase.arg, testcase.expected.c);
        });
      }
    });

    it('more values', () => {
      const inputArgs = [
        {type: 'string', value: 'test string %i string test'},
        {type: 'number', value: 1},
        {type: 'number', value: 2},
      ] satisfies Script.RemoteValue[];

      expect(logMessageFormatter.bind(undefined, inputArgs)).to.throw(
        'More value is provided: "test string %i string test 1 2"',
      );
    });

    it('less values', () => {
      const inputArgs = [
        {type: 'string', value: 'test string %i %i string test'},
        {type: 'number', value: 1},
      ] satisfies Script.RemoteValue[];
      const outputString =
        'Less value is provided: "test string %i %i string test 1"';

      expect(logMessageFormatter.bind(undefined, inputArgs)).to.throw(
        outputString,
      );
    });
  });
});
