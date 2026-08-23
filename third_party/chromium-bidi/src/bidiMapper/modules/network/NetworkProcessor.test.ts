/**
 * Copyright 2023 Google LLC.
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
import {describe, it} from 'node:test';
import {assert} from 'chai';

import {
  InvalidArgumentException,
  UnsupportedOperationException,
} from '../../../protocol/ErrorResponse.js';
import type {Network} from '../../../protocol/protocol.js';

import {NetworkProcessor, parseBiDiHeaders} from './NetworkProcessor.js';

describe('NetworkProcessor', () => {
  describe('parse url string', () => {
    it('invalid string', () => {
      assert.throws(
        () => NetworkProcessor.parseUrlString('invalid.url%%'),
        `Invalid URL 'invalid.url%%'`,
      );
    });

    it('valid string', () => {
      assert.doesNotThrow(() =>
        NetworkProcessor.parseUrlString('https://example.com/'),
      );
    });
  });

  describe('parse url patterns', () => {
    it('invalid string', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'string',
              pattern: 'invalid.url%%',
            },
          ]),
        `Invalid URL 'invalid.url%%'`,
      );
    });

    it('invalid hostname', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'pattern',
              hostname: 'abc/com/',
            },
          ]),
        `'/', '?', '#' are forbidden in hostname`,
      );
    });

    it('valid string', () => {
      assert.deepEqual(
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'string',
            pattern: 'https://example.com/',
          },
          {
            type: 'string',
            pattern: 'https://example.org/\\*',
          },
        ]),
        [
          {
            hostname: 'example.com',
            pathname: '/',
            port: '',
            protocol: 'https',
            search: '',
          },
          {
            hostname: 'example.org',
            pathname: '/*',
            port: '',
            protocol: 'https',
            search: '',
          },
        ],
      );
    });

    it('invalid pattern missing protocol', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'pattern',
              protocol: '',
              hostname: 'example.com',
            },
          ]),
        'URL pattern must specify a protocol',
      );
    });

    it('invalid pattern missing hostname', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'pattern',
              protocol: 'https',
              hostname: '',
            },
          ]),
        'URL pattern must specify a hostname',
      );
    });

    it('invalid pattern protocol cannot be a file', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'pattern',
              protocol: 'file',
              hostname: 'doc.txt',
            },
          ]),
        `URL pattern protocol cannot be 'file'`,
      );
    });

    it('invalid pattern hostname cannot contain a colon', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'pattern',
              protocol: 'https',
              hostname: 'a:b',
            },
          ]),
        `':' is only allowed inside brackets in hostname`,
      );
    });

    it('invalid pattern missing port', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'pattern',
              protocol: 'https',
              hostname: 'example.com',
              port: '',
            },
          ]),
        'URL pattern must specify a port',
      );
    });

    it('invalid pattern URL', () => {
      assert.throws(
        () =>
          NetworkProcessor.parseUrlPatterns([
            {
              type: 'pattern',
              protocol: '%',
            },
          ]),
        /Forbidden characters/,
      );
    });

    it('valid pattern', () => {
      assert.deepEqual(
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            pathname: '/',
            port: '443',
            search: '?q=search',
          },
        ]),
        [
          {
            protocol: 'https',
            hostname: 'example.com',
            pathname: '/',
            port: '',
            search: '?q=search',
          },
        ],
      );
    });

    it('valid pattern empty', () => {
      assert.deepEqual(NetworkProcessor.parseUrlPatterns([{type: 'pattern'}]), [
        {
          protocol: undefined,
          hostname: undefined,
          pathname: undefined,
          port: undefined,
          search: undefined,
        },
      ]);
    });
  });

  describe('isMethodValid', () => {
    it('validates method according to the HTTP spec', () => {
      assert.isTrue(NetworkProcessor.isMethodValid('GET'));
      assert.isTrue(NetworkProcessor.isMethodValid('DELETE'));
      assert.isTrue(NetworkProcessor.isMethodValid('1PUT'));
      assert.isTrue(NetworkProcessor.isMethodValid('*'));
      assert.isTrue(NetworkProcessor.isMethodValid('-'));
      assert.isFalse(NetworkProcessor.isMethodValid('{'));
      assert.isFalse(NetworkProcessor.isMethodValid('GET{'));
      assert.isFalse(NetworkProcessor.isMethodValid('\\'));
      assert.isFalse(NetworkProcessor.isMethodValid('"'));
      assert.isFalse(NetworkProcessor.isMethodValid(''));
    });
  });
  describe('parseBiDiHeaders', () => {
    const SOME_HEADER_NAME = 'SOME_HEADER_NAME';
    const SOME_HEADER_VALUE = 'SOME HEADER VALUE';
    const ANOTHER_HEADER_NAME = 'ANOTHER_HEADER_NAME';
    const ANOTHER_HEADER_VALUE = 'ANOTHER HEADER VALUE';

    it('should return an empty object for an empty array', () => {
      const result = parseBiDiHeaders([]);
      assert.deepEqual(result, {});
    });

    it('should parse a single header', () => {
      const headers: Network.Header[] = [
        {
          name: SOME_HEADER_NAME,
          value: {type: 'string', value: SOME_HEADER_VALUE},
        },
      ];
      const result = parseBiDiHeaders(headers);
      assert.deepEqual(result, {
        [SOME_HEADER_NAME]: SOME_HEADER_VALUE,
      });
    });

    it('should parse multiple unique headers', () => {
      const headers: Network.Header[] = [
        {
          name: SOME_HEADER_NAME,
          value: {type: 'string', value: SOME_HEADER_VALUE},
        },
        {
          name: ANOTHER_HEADER_NAME,
          value: {type: 'string', value: ANOTHER_HEADER_VALUE},
        },
      ];
      const result = parseBiDiHeaders(headers);
      assert.deepEqual(result, {
        [SOME_HEADER_NAME]: SOME_HEADER_VALUE,
        [ANOTHER_HEADER_NAME]: ANOTHER_HEADER_VALUE,
      });
    });

    it('should not combine multiple headers', () => {
      const headers: Network.Header[] = [
        {
          name: SOME_HEADER_NAME,
          value: {type: 'string', value: 'SOME VALUE'},
        },
        {
          name: SOME_HEADER_NAME,
          value: {type: 'string', value: 'ANOTHER VALUE'},
        },
        {
          name: SOME_HEADER_NAME,
          value: {type: 'string', value: 'THE LAST VALUE'},
        },
      ];
      const result = parseBiDiHeaders(headers);
      assert.deepEqual(result, {
        [SOME_HEADER_NAME]: 'THE LAST VALUE',
      });
    });

    it('should throw for non-string header values', () => {
      const headers: Network.Header[] = [
        {
          name: SOME_HEADER_NAME,
          value: {type: 'base64', value: '...'},
        },
      ];
      assert.throws(
        () => parseBiDiHeaders(headers),
        UnsupportedOperationException,
        'Only string headers values are supported',
      );
    });

    it('should be case-sensitive for header names', () => {
      const headers: Network.Header[] = [
        {
          name: 'SOME_HEADER_NAME',
          value: {type: 'string', value: 'SOME VALUE'},
        },
        {
          name: 'some_header_name',
          value: {type: 'string', value: 'some value'},
        },
      ];
      const result = parseBiDiHeaders(headers);
      assert.deepEqual(result, {
        ['SOME_HEADER_NAME']: 'SOME VALUE',
        some_header_name: 'some value',
      });
    });
    it('should throw for invalid header name', () => {
      const headers: Network.Header[] = [
        {
          name: 'SOME HEADER NAME',
          value: {type: 'string', value: 'SOME VALUE'},
        },
        {
          name: 'invalid name\n',
          value: {type: 'string', value: 'some value'},
        },
      ];
      assert.throws(() => parseBiDiHeaders(headers), InvalidArgumentException);
    });

    it('should throw for invalid header value', () => {
      const headers: Network.Header[] = [
        {
          name: 'SOME HEADER NAME',
          value: {type: 'string', value: 'SOME VALUE'},
        },
        {
          name: 'valid-name',
          value: {type: 'string', value: 'invalid value\n'},
        },
      ];
      assert.throws(() => parseBiDiHeaders(headers), InvalidArgumentException);
    });
  });
});
