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

import {expect} from 'chai';

import {urlMatchesAboutBlank} from './UrlHelpers.js';

describe('BrowsingContextStorage', () => {
  describe('urlMatchesAboutBlank', () => {
    describe('should return true for matching urls', () => {
      it('"about:blank"', () => {
        expect(urlMatchesAboutBlank('about:blank')).to.be.true;
      });

      it('"about:blank?foo=bar"', () => {
        expect(urlMatchesAboutBlank('about:blank?foo=bar')).to.be.true;
      });

      it('"about:blank#foo"', () => {
        expect(urlMatchesAboutBlank('about:blank#foo')).to.be.true;
      });

      it('"about:Blank"', () => {
        expect(urlMatchesAboutBlank('about:Blank')).to.be.true;
      });

      it('empty string', () => {
        expect(urlMatchesAboutBlank('')).to.be.true;
      });
    });

    describe('should return false for not matching urls', () => {
      it('"http://example.com"', () => {
        expect(urlMatchesAboutBlank('http://example.com')).to.be.false;
      });

      it('"about:blanka"', () => {
        expect(urlMatchesAboutBlank('about:blanka')).to.be.false;
      });

      it('"about:blank/foo"', () => {
        expect(urlMatchesAboutBlank('about:blank/foo')).to.be.false;
      });

      it('"about://blank"', () => {
        expect(urlMatchesAboutBlank('about://blank')).to.be.false;
      });

      it('"about: blank"', () => {
        expect(urlMatchesAboutBlank('about: blank')).to.be.false;
      });

      it('username and password', () => {
        expect(urlMatchesAboutBlank('about:username:password@blank')).to.be
          .false;
      });

      it('null', () => {
        expect(urlMatchesAboutBlank(null as any)).to.be.false;
      });

      it('undefined', () => {
        expect(urlMatchesAboutBlank(undefined as any)).to.be.false;
      });

      it('"blob:http://example.com"', () => {
        expect(urlMatchesAboutBlank('blob:http://example.com')).to.be.false;
      });

      it('"about:srcdoc"', () => {
        expect(urlMatchesAboutBlank('about:srcdoc')).to.be.false;
      });
    });
  });
});
