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
        `Invalid URL 'invalid.url%%'`,
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
        ]),
      ).to.throw(`Invalid URL 'invalid.url%%'`);
    });

    it('invalid hostname', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            hostname: 'abc/com/',
          },
        ]),
      ).to.throw(`'/', '?', '#' are forbidden in hostname`);
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
            pattern: 'https://example.org/\\*',
          },
        ]),
      ).to.deep.equal([
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
        ]),
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
        ]),
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
        ]),
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
        ]),
      ).to.throw(`':' is only allowed inside brackets in hostname`);
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
        ]),
      ).to.throw('URL pattern must specify a port');
    });

    it('invalid pattern URL', () => {
      expect(() =>
        NetworkProcessor.parseUrlPatterns([
          {
            type: 'pattern',
            protocol: '%',
          },
        ]),
      ).to.throw(/Forbidden characters/);
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
        ]),
      ).to.deep.equal([
        {
          protocol: 'https',
          hostname: 'example.com',
          pathname: '/',
          port: '',
          search: '?q=search',
        },
      ]);
    });

    it('valid pattern empty', () => {
      expect(
        NetworkProcessor.parseUrlPatterns([{type: 'pattern'}]),
      ).to.deep.equal([
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
      expect(NetworkProcessor.isMethodValid('GET')).to.be.true;
      expect(NetworkProcessor.isMethodValid('DELETE')).to.be.true;
      expect(NetworkProcessor.isMethodValid('1PUT')).to.be.true;
      expect(NetworkProcessor.isMethodValid('*')).to.be.true;
      expect(NetworkProcessor.isMethodValid('-')).to.be.true;
      expect(NetworkProcessor.isMethodValid('{')).to.be.false;
      expect(NetworkProcessor.isMethodValid('GET{')).to.be.false;
      expect(NetworkProcessor.isMethodValid('\\')).to.be.false;
      expect(NetworkProcessor.isMethodValid('"')).to.be.false;
      expect(NetworkProcessor.isMethodValid('')).to.be.false;
    });
  });
});
