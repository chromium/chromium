/*
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

import {isSingleComplexGrapheme, isSingleGrapheme} from './GraphemeTools.js';

describe('GraphemeTools', () => {
  describe('isSingleGrapheme', () => {
    describe('should return true for a single grapheme', () => {
      it('"a", a single char', () => {
        expect(isSingleGrapheme('a')).to.be.true;
      });

      it('"😄", a single surrogate codepoint', () => {
        expect(isSingleGrapheme('\ud83d\ude04')).to.be.true;
      });

      it('"நி", a grapheme containing several chars', () => {
        expect(isSingleGrapheme('\u0BA8\u0BBF')).to.be.true;
      });

      it('"각", a grapheme containing several chars', () => {
        expect(isSingleGrapheme('\u1100\u1161\u11A8')).to.be.true;
      });

      it('"❤️", a grapheme containing several codepoints', () => {
        expect(isSingleGrapheme('\u2764\ufe0f')).to.be.true;
      });
    });

    describe('should return false for multiple graphemes', () => {
      it('2 symbols', () => {
        expect(isSingleGrapheme('fa')).to.be.false;
      });

      it('"😄a" a codepoint with a symbol', () => {
        expect(isSingleGrapheme('\ud83d\ude04a')).to.be.false;
      });

      it('"நிa" a grapheme with a symbol', () => {
        expect(isSingleGrapheme('\u0BA8\u0BBFa')).to.be.false;
      });

      it('"각a" a grapheme with a symbol', () => {
        expect(isSingleGrapheme('\u1100\u1161\u11A8a')).to.be.false;
      });

      it('"❤️a" a grapheme with a symbol', () => {
        expect(isSingleGrapheme('\u2764\ufe0fa')).to.be.false;
      });

      it('"😄😍" 2 graphemes', () => {
        expect(isSingleGrapheme('\ud83d\ude04\ud83d\ude0d')).to.be.false;
      });

      it('"ch" 2 graphemes', () => {
        // https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
        // Spec says it's a single grapheme in slovak locale. We support only `en` locale.
        expect(isSingleGrapheme('\ud83d\ude04\ud83d\ude0d')).to.be.false;
      });
    });
  });

  describe('isSingleComplexGrapheme', () => {
    describe('should return true', () => {
      it('"😄", a single surrogate codepoint', () => {
        expect(isSingleComplexGrapheme('\ud83d\ude04')).to.be.true;
      });

      it('"நி", a grapheme containing several chars', () => {
        expect(isSingleComplexGrapheme('\u0BA8\u0BBF')).to.be.true;
      });

      it('"각", a grapheme containing several chars', () => {
        expect(isSingleComplexGrapheme('\u1100\u1161\u11A8')).to.be.true;
      });

      it('"❤️", a grapheme containing several codepoints', () => {
        expect(isSingleComplexGrapheme('\u2764\ufe0f')).to.be.true;
      });
    });

    describe('should return false', () => {
      it('"AB", 2 symbols', () => {
        expect(isSingleComplexGrapheme('AB')).to.be.false;
      });

      it('"A", a single simple symbol', () => {
        expect(isSingleComplexGrapheme('A')).to.be.false;
      });

      it('"1", a single simple symbol', () => {
        expect(isSingleComplexGrapheme('1')).to.be.false;
      });

      it('empty string', () => {
        expect(isSingleComplexGrapheme('')).to.be.false;
      });
    });
  });
});
