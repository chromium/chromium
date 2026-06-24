/*
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
 *
 */
import {describe, it} from 'node:test';
import {assert} from 'chai';
import type {Protocol} from 'devtools-protocol';

import type {Network} from '../../../protocol/protocol.js';

import {NetworkProcessor} from './NetworkProcessor.js';
import * as networkUtils from './NetworkUtils.js';

describe('NetworkUtils', () => {
  describe('should compute response headers size', () => {
    it('empty', () => {
      const headers: Network.Header[] = [];
      const responseHeadersSize = networkUtils.computeHeadersSize(headers);

      assert.equal(responseHeadersSize, 0);
    });

    it('non-empty', () => {
      const headers: Network.Header[] = [
        {
          name: 'Content-Type',
          value: {
            type: 'string',
            value: 'text/html',
          },
        },
        {
          name: 'Content-Length',
          value: {
            type: 'string',
            value: '123',
          },
        },
      ];
      const responseHeadersSize = networkUtils.computeHeadersSize(headers);

      assert.equal(responseHeadersSize, 46);
    });
  });

  describe('should convert CDP network headers to Bidi network headers', () => {
    it('empty', () => {
      const cdpNetworkHeaders = {};
      const bidiNetworkHeaders =
        networkUtils.bidiNetworkHeadersFromCdpNetworkHeaders(cdpNetworkHeaders);

      assert.deepEqual(bidiNetworkHeaders, []);
    });

    it('non-empty', () => {
      const cdpNetworkHeaders = {
        'Content-Type': 'text/html',
        'Content-Length': '123',
      };
      const bidiNetworkHeaders =
        networkUtils.bidiNetworkHeadersFromCdpNetworkHeaders(cdpNetworkHeaders);

      assert.deepEqual(bidiNetworkHeaders, [
        {
          name: 'Content-Type',
          value: {
            type: 'string',
            value: 'text/html',
          },
        },
        {
          name: 'Content-Length',
          value: {
            type: 'string',
            value: '123',
          },
        },
      ]);
    });
  });

  describe('should convert Bidi network headers to CDP network headers', () => {
    it('undefined', () => {
      const cdpNetworkHeaders =
        networkUtils.cdpNetworkHeadersFromBidiNetworkHeaders(undefined);

      assert.equal(cdpNetworkHeaders, undefined);
    });

    it('empty', () => {
      const bidiNetworkHeaders: Network.Header[] = [];
      const cdpNetworkHeaders =
        networkUtils.cdpNetworkHeadersFromBidiNetworkHeaders(
          bidiNetworkHeaders,
        );

      assert.deepEqual(cdpNetworkHeaders, {});
    });

    it('non-empty', () => {
      const bidiNetworkHeaders: Network.Header[] = [
        {
          name: 'Content-Type',
          value: {
            type: 'string',
            value: 'text/html',
          },
        },
        {
          name: 'Content-Length',
          value: {
            type: 'string',
            value: '123',
          },
        },
      ];
      const cdpNetworkHeaders =
        networkUtils.cdpNetworkHeadersFromBidiNetworkHeaders(
          bidiNetworkHeaders,
        );

      assert.deepEqual(cdpNetworkHeaders, {
        'Content-Type': 'text/html',
        'Content-Length': '123',
      });
    });
  });

  describe('should convert CDP fetch header entry array to Bidi network headers', () => {
    it('empty', () => {
      const cdpFetchHeaderEntryArray: Protocol.Fetch.HeaderEntry[] = [];
      const bidiNetworkHeaders =
        networkUtils.bidiNetworkHeadersFromCdpFetchHeaders(
          cdpFetchHeaderEntryArray,
        );

      assert.deepEqual(bidiNetworkHeaders, []);
    });

    it('non-empty', () => {
      const cdpFetchHeaderEntryArray: Protocol.Fetch.HeaderEntry[] = [
        {
          name: 'Content-Type',
          value: 'text/html',
        },
        {
          name: 'Content-Length',
          value: '123',
        },
      ];
      const bidiNetworkHeaders =
        networkUtils.bidiNetworkHeadersFromCdpFetchHeaders(
          cdpFetchHeaderEntryArray,
        );

      assert.deepEqual(bidiNetworkHeaders, [
        {
          name: 'Content-Type',
          value: {
            type: 'string',
            value: 'text/html',
          },
        },
        {
          name: 'Content-Length',
          value: {
            type: 'string',
            value: '123',
          },
        },
      ]);
    });
  });

  describe('should convert Bidi network headers to CDP fetch header entry array', () => {
    it('undefined', () => {
      const cdpFetchHeaderEntryArray =
        networkUtils.cdpFetchHeadersFromBidiNetworkHeaders(undefined);

      assert.equal(cdpFetchHeaderEntryArray, undefined);
    });

    it('empty', () => {
      const bidiNetworkHeaders: Network.Header[] = [];
      const cdpFetchHeaderEntryArray =
        networkUtils.cdpFetchHeadersFromBidiNetworkHeaders(bidiNetworkHeaders);

      assert.deepEqual(cdpFetchHeaderEntryArray, []);
    });

    it('non-empty', () => {
      const bidiNetworkHeaders: Network.Header[] = [
        {
          name: 'Content-Type',
          value: {
            type: 'string',
            value: 'text/html',
          },
        },
        {
          name: 'Content-Length',
          value: {
            type: 'string',
            value: '123',
          },
        },
      ];
      const cdpFetchHeaderEntryArray =
        networkUtils.cdpFetchHeadersFromBidiNetworkHeaders(bidiNetworkHeaders);

      assert.deepEqual(cdpFetchHeaderEntryArray, [
        {
          name: 'Content-Type',
          value: 'text/html',
        },
        {
          name: 'Content-Length',
          value: '123',
        },
      ]);
    });
  });

  describe('getTimings', () => {
    it('should work with undefined', () => {
      assert.equal(networkUtils.getTiming(undefined), 0);
    });

    it('should work with negative numbers', () => {
      assert.equal(networkUtils.getTiming(-1), 0);
    });

    it('should work with ints', () => {
      assert.equal(networkUtils.getTiming(1), 1);
    });
  });

  describe('matchUrlPattern', () => {
    function createPattern(pattern: Network.UrlPattern) {
      return NetworkProcessor.parseUrlPatterns([pattern])[0]!;
    }

    it('should not match urls', () => {
      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'string',
            pattern: 'https://example.test/test?query',
          }),
          'https://example2.test/test?query',
        ),
        false,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'string',
            pattern: 'https://example.test:333',
          }),
          'https://example.test:444',
        ),
        false,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({search: '', type: 'pattern'}),
          'https://web-platform.test/?search',
        ),
        false,
      );
    });

    it('should match urls against string patterns', () => {
      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'string',
            pattern: 'https://example.test/test?query',
          }),
          'https://example.test/test?query',
        ),
        true,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'string',
            pattern: 'https://www.example.com/',
          }),
          'https://www.example.com/',
        ),
        true,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'string',
            pattern: 'https://example.test:333',
          }),
          'https://example.test:333',
        ),
        true,
      );
    });

    it('should match urls against object patterns', () => {
      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.test',
            port: '333',
            pathname: '/test',
            search: '?query',
          }),
          'https://example.test:333/test?query',
        ),
        true,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'pattern',
            search: '?query',
          }),
          'https://example.test:333/test?query',
        ),
        true,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({hostname: 'WEB-PLATFORM.TEST', type: 'pattern'}),
          'https://web-platform.test/',
        ),
        true,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.test',
            port: '333',
            pathname: '/test',
            search: '?query+another',
          }),
          'https://example.test:333/test?query+another',
        ),
        true,
      );

      assert.equal(
        networkUtils.matchUrlPattern(
          createPattern({
            type: 'pattern',
            protocol: 'https',
            hostname: 'www.example.com',
            pathname: '/',
          }),
          'https://www.example.com/',
        ),
        true,
      );
    });
  });
});
