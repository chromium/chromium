/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.spell.SpellCheckTest');
goog.setTestOnly();

const SpellCheck = goog.require('goog.spell.SpellCheck');
const testSuite = goog.require('goog.testing.testSuite');

const TEST_DATA = {
  'Test': [SpellCheck.WordStatus.VALID, []],
  'strnig': [SpellCheck.WordStatus.INVALID, []],
  'wtih': [SpellCheck.WordStatus.INVALID, []],
  'a': [SpellCheck.WordStatus.VALID, []],
  'few': [SpellCheck.WordStatus.VALID, []],
  'misspeled': [
    SpellCheck.WordStatus.INVALID,
    ['misspelled', 'misapplied', 'misspell'],
  ],
  'words': [SpellCheck.WordStatus.VALID, []],
  'Testing': [SpellCheck.WordStatus.VALID, []],
  'set': [SpellCheck.WordStatus.VALID, []],
  'status': [SpellCheck.WordStatus.VALID, []],
  'vaild': [SpellCheck.WordStatus.INVALID, []],
  'invalid': [SpellCheck.WordStatus.VALID, []],
  'ignoerd': [SpellCheck.WordStatus.INVALID, []],
};

/**
 * @param {!Array<string>} words
 * @param {!Object} spellChecker
 * @param {!Function} callback
 */
function mockSpellCheckingFunction(words, spellChecker, callback) {
  const len = words.length;
  const data = [];
  for (let i = 0; i < len; i++) {
    const word = words[i];
    const status = TEST_DATA[word][0];
    const suggestions = TEST_DATA[word][1];
    data.push([word, status, suggestions]);
  }
  callback.call(spellChecker, data);
}

testSuite({
  testWordMatching() {
    const spell = new SpellCheck(mockSpellCheckingFunction);

    const valid = SpellCheck.WordStatus.VALID;
    const invalid = SpellCheck.WordStatus.INVALID;

    spell.checkBlock('Test strnig wtih a few misspeled words.');
    assertEquals(valid, spell.checkWord('Test'));
    assertEquals(invalid, spell.checkWord('strnig'));
    assertEquals(invalid, spell.checkWord('wtih'));
    assertEquals(valid, spell.checkWord('a'));
    assertEquals(valid, spell.checkWord('few'));
    assertEquals(invalid, spell.checkWord('misspeled'));
    assertEquals(valid, spell.checkWord('words'));
  },

  testSetWordStatusValid() {
    const spell = new SpellCheck(mockSpellCheckingFunction);

    const valid = SpellCheck.WordStatus.VALID;

    spell.checkBlock('Testing set status vaild.');
    spell.setWordStatus('vaild', valid);

    assertEquals(valid, spell.checkWord('vaild'));
  },

  testSetWordStatusInvalid() {
    const spell = new SpellCheck(mockSpellCheckingFunction);

    const invalid = SpellCheck.WordStatus.INVALID;

    spell.checkBlock('Testing set status invalid.');
    spell.setWordStatus('invalid', invalid);

    assertEquals(invalid, spell.checkWord('invalid'));
  },

  testSetWordStatusIgnored() {
    const spell = new SpellCheck(mockSpellCheckingFunction);

    const ignored = SpellCheck.WordStatus.IGNORED;

    spell.checkBlock('Testing set status ignoerd.');
    spell.setWordStatus('ignoerd', ignored);

    assertEquals(ignored, spell.checkWord('ignoerd'));
  },

  testGetSuggestions() {
    const spell = new SpellCheck(mockSpellCheckingFunction);

    spell.checkBlock('Test strnig wtih a few misspeled words.');
    const suggestions = spell.getSuggestions('misspeled');
    assertEquals(3, suggestions.length);
  },

  testWordBoundaryRegex() {
    const regex = SpellCheck.WORD_BOUNDARY_REGEX;
    assertEquals(3, 'one two three'.split(regex).length);
    assertEquals(3, 'one/two/three'.split(regex).length);
    assertEquals(3, 'one-two-three'.split(regex).length);
    assertEquals(3, 'one.two.three'.split(regex).length);
    assertEquals(3, 'one,two,three'.split(regex).length);
    assertEquals(3, 'one[two]three'.split(regex).length);
    assertEquals(3, 'one\\two\\three'.split(regex).length);
    assertEquals(3, 'one\rtwo\nthree'.split(regex).length);
    assertEquals(3, 'one\ttwo\tthree'.split(regex).length);
    assertEquals(3, 'one!two~three'.split(regex).length);
    assertEquals(18, 'A"1#B$2%C!3(D)4*E+5:F;6<G>7=H?8@I^9'.split(regex).length);
    assertEquals(6, 'a_two`z{four|Z}six'.split(regex).length);
  },
});
