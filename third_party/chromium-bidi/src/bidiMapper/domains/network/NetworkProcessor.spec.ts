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
import {expect} from 'chai';

import {NetworkProcessor} from './NetworkProcessor.js';

describe('NetworkProcessor', () => {
  describe('parse url string', () => {
    it('invalid string', () => {
      expect(() => NetworkProcessor.parseUrlString('invalid.url%%')).to.throw(
        `Invalid URL 'invalid.url%%'`
      );
    });

    it('valid string', () => {
      expect(NetworkProcessor.parseUrlString('https://example.com/')).not.to
        .throw;
    });
  });

  describe('parse url patterns', () => {
    it('invalid string', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'string',
            pattern: 'invalid.url%%',
          },
        ])
      ).to.throw(`Invalid URL 'invalid.url%%'`);
    });

    it('valid string', () => {
      expect(
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'string',
            pattern: 'https://example.com/',
          },
          {
            type: 'string',
            pattern: 'https://example.org/*',
          },
        ])
      ).to.deep.equal([
        {
          type: 'string',
          pattern: 'https://example.com/',
        },
        {
          type: 'string',
          pattern: 'https://example.org/*',
        },
      ]);
    });

    it('invalid pattern missing protocol', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: '',
            hostname: 'example.com',
          },
        ])
      ).to.throw('URL pattern must specify a protocol');
    });

    it('invalid pattern missing hostname', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: 'https',
            hostname: '',
          },
        ])
      ).to.throw('URL pattern must specify a hostname');
    });

    it('invalid pattern protocol cannot be a file', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: 'file',
            hostname: 'doc.txt',
          },
        ])
      ).to.throw(`URL pattern protocol cannot be 'file'`);
    });

    it('invalid pattern hostname cannot contain a colon', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: 'https',
            hostname: 'a:b',
          },
        ])
      ).to.throw('URL pattern hostname must not contain a colon');
    });

    it('invalid pattern missing port', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            port: '',
          },
        ])
      ).to.throw('URL pattern must specify a port');
    });

    it('invalid pattern URL', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: '%',
          },
        ])
      ).to.throw(/TypeError.*: Invalid URL/);
    });

    it('valid pattern', () => {
      expect(
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: 'https',
            hostname: 'example.com',
            pathname: '/',
            port: '443',
            search: '?q=search',
          },
        ])
      ).to.deep.equal([
        {
          type: 'pattern',
          protocol: 'https',
          hostname: 'example.com',
          pathname: '/',
          port: '443',
          search: '?q=search',
        },
      ]);
    });

    it('valid pattern empty', () => {
      expect(
        NetworkProcessor.parseUrlPatterns([{type: 'pattern'}])
      ).to.deep.equal([
        {
          type: 'pattern',
        },
      ]);
    });
  });
});
