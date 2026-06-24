/**
 * Copyright 2024 Google LLC.
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

import {urlMatchesAboutBlank} from './urlHelpers.js';

describe('BrowsingContextStorage', () => {
  describe('urlMatchesAboutBlank', () => {
    describe('should return true for matching urls', () => {
      it('"about:blank"', () => {
        assert.isTrue(urlMatchesAboutBlank('about:blank'));
      });

      it('"about:blank?foo=bar"', () => {
        assert.isTrue(urlMatchesAboutBlank('about:blank?foo=bar'));
      });

      it('"about:blank#foo"', () => {
        assert.isTrue(urlMatchesAboutBlank('about:blank#foo'));
      });

      it('"about:Blank"', () => {
        assert.isTrue(urlMatchesAboutBlank('about:Blank'));
      });

      it('empty string', () => {
        assert.isTrue(urlMatchesAboutBlank(''));
      });
    });

    describe('should return false for not matching urls', () => {
      it('"http://example.com"', () => {
        assert.isFalse(urlMatchesAboutBlank('http://example.com'));
      });

      it('"about:blanka"', () => {
        assert.isFalse(urlMatchesAboutBlank('about:blanka'));
      });

      it('"about:blank/foo"', () => {
        assert.isFalse(urlMatchesAboutBlank('about:blank/foo'));
      });

      it('"about://blank"', () => {
        assert.isFalse(urlMatchesAboutBlank('about://blank'));
      });

      it('"about: blank"', () => {
        assert.isFalse(urlMatchesAboutBlank('about: blank'));
      });

      it('username and password', () => {
        assert.isFalse(urlMatchesAboutBlank('about:username:password@blank'));
      });

      it('null', () => {
        assert.isFalse(urlMatchesAboutBlank(null as any));
      });

      it('undefined', () => {
        assert.isFalse(urlMatchesAboutBlank(undefined as any));
      });

      it('"blob:http://example.com"', () => {
        assert.isFalse(urlMatchesAboutBlank('blob:http://example.com'));
      });

      it('"about:srcdoc"', () => {
        assert.isFalse(urlMatchesAboutBlank('about:srcdoc'));
      });
    });
  });
});
