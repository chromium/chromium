/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.i18n.CharListDecompressorTest');
goog.setTestOnly();

const CharListDecompressor = goog.require('goog.i18n.CharListDecompressor');
const testSuite = goog.require('goog.testing.testSuite');

const decompressor = new CharListDecompressor();

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testBuildCharMap() {
    assertEquals(0, decompressor.charMap_['0']);
    assertEquals(10, decompressor.charMap_['A']);
    assertEquals(87, decompressor.charMap_['}']);
  },

  testGetCodeAt() {
    /** @suppress {visibility} suppression added to enable type checking */
    const code = decompressor.getCodeAt_('321', 1, 2);
    assertEquals(90, code);
  },

  testAddCharsForType0() {
    const list = ['a'];
    /** @suppress {visibility} suppression added to enable type checking */
    const lastcode = decompressor.addChars_(list, 97, 0, 0);
    assertArrayEquals(['a', 'b'], list);
    assertEquals(98, lastcode);
  },

  testAddCharsForType1() {
    const list = ['a'];
    /** @suppress {visibility} suppression added to enable type checking */
    const lastcode = decompressor.addChars_(list, 98, 0, 1);
    assertArrayEquals(['a', 'a'], list);
    assertEquals(97, lastcode);
  },

  testAddCharsForType2() {
    const list = ['a'];
    /** @suppress {visibility} suppression added to enable type checking */
    const lastcode = decompressor.addChars_(list, 97, 1, 2);
    assertArrayEquals(['a', 'b', 'c'], list);
    assertEquals(99, lastcode);
  },

  testToCharList() {
    const list = decompressor.toCharList('%812E<E');  // a, x-z, p-r
    assertArrayEquals(['a', 'x', 'y', 'z', 'p', 'q', 'r'], list);
  },
});
