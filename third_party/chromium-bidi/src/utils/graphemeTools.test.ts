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

import {describe, it} from 'node:test';
import {assert} from 'chai';

import {isSingleComplexGrapheme, isSingleGrapheme} from './graphemeTools.js';

describe('GraphemeTools', () => {
  describe('isSingleGrapheme', () => {
    describe('should return true for a single grapheme', () => {
      it('"a", a single char', () => {
        assert.isTrue(isSingleGrapheme('a'));
      });

      it('"😄", a single surrogate codepoint', () => {
        assert.isTrue(isSingleGrapheme('\ud83d\ude04'));
      });

      it('"நி", a grapheme containing several chars', () => {
        assert.isTrue(isSingleGrapheme('\u0BA8\u0BBF'));
      });

      it('"각", a grapheme containing several chars', () => {
        assert.isTrue(isSingleGrapheme('\u1100\u1161\u11A8'));
      });

      it('"❤️", a grapheme containing several codepoints', () => {
        assert.isTrue(isSingleGrapheme('\u2764\ufe0f'));
      });
    });

    describe('should return false for multiple graphemes', () => {
      it('2 symbols', () => {
        assert.isFalse(isSingleGrapheme('fa'));
      });

      it('"😄a" a codepoint with a symbol', () => {
        assert.isFalse(isSingleGrapheme('\ud83d\ude04a'));
      });

      it('"நிa" a grapheme with a symbol', () => {
        assert.isFalse(isSingleGrapheme('\u0BA8\u0BBFa'));
      });

      it('"각a" a grapheme with a symbol', () => {
        assert.isFalse(isSingleGrapheme('\u1100\u1161\u11A8a'));
      });

      it('"❤️a" a grapheme with a symbol', () => {
        assert.isFalse(isSingleGrapheme('\u2764\ufe0fa'));
      });

      it('"😄😍" 2 graphemes', () => {
        assert.isFalse(isSingleGrapheme('\ud83d\ude04\ud83d\ude0d'));
      });

      it('"ch" 2 graphemes', () => {
        // https://www.unicode.org/reports/tr29/#Grapheme_Cluster_Boundaries
        // Spec says it's a single grapheme in slovak locale. We support only `en` locale.
        assert.isFalse(isSingleGrapheme('\ud83d\ude04\ud83d\ude0d'));
      });
    });
  });

  describe('isSingleComplexGrapheme', () => {
    describe('should return true', () => {
      it('"😄", a single surrogate codepoint', () => {
        assert.isTrue(isSingleComplexGrapheme('\ud83d\ude04'));
      });

      it('"நி", a grapheme containing several chars', () => {
        assert.isTrue(isSingleComplexGrapheme('\u0BA8\u0BBF'));
      });

      it('"각", a grapheme containing several chars', () => {
        assert.isTrue(isSingleComplexGrapheme('\u1100\u1161\u11A8'));
      });

      it('"❤️", a grapheme containing several codepoints', () => {
        assert.isTrue(isSingleComplexGrapheme('\u2764\ufe0f'));
      });
    });

    describe('should return false', () => {
      it('"AB", 2 symbols', () => {
        assert.isFalse(isSingleComplexGrapheme('AB'));
      });

      it('"A", a single simple symbol', () => {
        assert.isFalse(isSingleComplexGrapheme('A'));
      });

      it('"1", a single simple symbol', () => {
        assert.isFalse(isSingleComplexGrapheme('1'));
      });

      it('empty string', () => {
        assert.isFalse(isSingleComplexGrapheme(''));
      });
    });
  });
});
