/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.structs.InversionMapTest');
goog.setTestOnly();

const InversionMap = goog.require('goog.structs.InversionMap');
const testSuite = goog.require('goog.testing.testSuite');

function newAsciiMap() {
  return new InversionMap([0, 97, 98, 99, 100, 101, 120, 121, 122, 123], [
    null,
    'LATIN SMALL LETTER A',
    'LATIN SMALL LETTER B',
    'LATIN SMALL LETTER C',
    'LATIN SMALL LETTER D',
    null,
    'LATIN SMALL LETTER X',
    'LATIN SMALL LETTER Y',
    'LATIN SMALL LETTER Z',
    null,
  ]);
}
testSuite({
  testInversionWithDelta() {
    const alphabetNames = new InversionMap(
        [0, 97, 1, 1, 1, 20, 1, 1, 1],
        [
          null,
          'LATIN SMALL LETTER A',
          'LATIN SMALL LETTER B',
          'LATIN SMALL LETTER C',
          null,
          'LATIN SMALL LETTER X',
          'LATIN SMALL LETTER Y',
          'LATIN SMALL LETTER Z',
          null,
        ],
        true);

    assertEquals('LATIN SMALL LETTER A', alphabetNames.at(97));
    assertEquals('LATIN SMALL LETTER Y', alphabetNames.at(121));
    assertEquals(null, alphabetNames.at(140));
    assertEquals(null, alphabetNames.at(0));
  },

  testInversionWithoutDelta() {
    const alphabetNames = new InversionMap(
        [0, 97, 98, 99, 100, 120, 121, 122, 123],
        [
          null,
          'LATIN SMALL LETTER A',
          'LATIN SMALL LETTER B',
          'LATIN SMALL LETTER C',
          null,
          'LATIN SMALL LETTER X',
          'LATIN SMALL LETTER Y',
          'LATIN SMALL LETTER Z',
          null,
        ],
        false);

    assertEquals('LATIN SMALL LETTER A', alphabetNames.at(97));
    assertEquals('LATIN SMALL LETTER Y', alphabetNames.at(121));
    assertEquals(null, alphabetNames.at(140));
    assertEquals(null, alphabetNames.at(0));
  },

  testInversionWithoutDeltaNoOpt() {
    const alphabetNames =
        new InversionMap([0, 97, 98, 99, 100, 120, 121, 122, 123], [
          null,
          'LATIN SMALL LETTER A',
          'LATIN SMALL LETTER B',
          'LATIN SMALL LETTER C',
          null,
          'LATIN SMALL LETTER X',
          'LATIN SMALL LETTER Y',
          'LATIN SMALL LETTER Z',
          null,
        ]);

    assertEquals('LATIN SMALL LETTER A', alphabetNames.at(97));
    assertEquals('LATIN SMALL LETTER Y', alphabetNames.at(121));
    assertEquals(null, alphabetNames.at(140));
    assertEquals(null, alphabetNames.at(0));
  },

  testInversionMapSplice1() {
    const alphabetNames = newAsciiMap();
    alphabetNames.spliceInversion([99, 105, 114], ['XXX', 'YYY', 'ZZZ']);
    assertEquals('LATIN SMALL LETTER B', alphabetNames.at(98));
    assertEquals('XXX', alphabetNames.at(100));
    assertEquals('ZZZ', alphabetNames.at(114));
    assertEquals('ZZZ', alphabetNames.at(119));
    assertEquals('LATIN SMALL LETTER X', alphabetNames.at(120));
  },

  testInversionMapSplice2() {
    const alphabetNames = newAsciiMap();
    alphabetNames.spliceInversion([105, 114, 121], ['XXX', 'YYY', 'ZZZ']);
    assertEquals(null, alphabetNames.at(104));
    assertEquals('XXX', alphabetNames.at(105));
    assertEquals('YYY', alphabetNames.at(120));
    assertEquals('ZZZ', alphabetNames.at(121));
    assertEquals('LATIN SMALL LETTER Z', alphabetNames.at(122));
  },

  testInversionMapSplice3() {
    const alphabetNames = newAsciiMap();
    alphabetNames.spliceInversion(
        [98, 99], ['CHANGED LETTER B', 'CHANGED LETTER C']);
    assertEquals('LATIN SMALL LETTER A', alphabetNames.at(97));
    assertEquals('CHANGED LETTER B', alphabetNames.at(98));
    assertEquals('CHANGED LETTER C', alphabetNames.at(99));
    assertEquals('LATIN SMALL LETTER D', alphabetNames.at(100));
    assertEquals(null, alphabetNames.at(101));
  },

  testInversionMapSplice4() {
    const alphabetNames = newAsciiMap();
    alphabetNames.spliceInversion(
        [98, 1], ['CHANGED LETTER B', 'CHANGED LETTER C'],
        true /* delta mode */);
    assertEquals('LATIN SMALL LETTER A', alphabetNames.at(97));
    assertEquals('CHANGED LETTER B', alphabetNames.at(98));
    assertEquals('CHANGED LETTER C', alphabetNames.at(99));
    assertEquals('LATIN SMALL LETTER D', alphabetNames.at(100));
    assertEquals(null, alphabetNames.at(101));
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testInversionMapSplice5() {
    const map = new InversionMap([0, 97, 98, 99], [
      null,
      'LATIN SMALL LETTER A',
      'LATIN SMALL LETTER B',
      'LATIN SMALL LETTER C',
    ]);
    map.spliceInversion([98], ['CHANGED LETTER B']);
    assertEquals('LATIN SMALL LETTER A', map.at(97));
    assertEquals('CHANGED LETTER B', map.at(98));
    assertEquals('LATIN SMALL LETTER C', map.at(99));

    assertArrayEquals([0, 97, 98, 99], map.rangeArray);
  },

  testInversionMapSpliceLarge() {
    const map = new InversionMap([0, 99, 100, 101], [null, true, false, null]);
    const rangeArray = [];
    const values = [];
    for (let i = 100, value = true; i < 1000000; i++, value = !value) {
      rangeArray.push(i);
      values.push(value);
    }
    map.spliceInversion(rangeArray, values);
    assertEquals(null, map.at(98));
    assertEquals(true, map.at(99));
    assertEquals(true, map.at(100));
    assertEquals(false, map.at(101));
    assertEquals(true, map.at(102));
    assertEquals(false, map.at(103));
    assertEquals(false, map.at(999997));
    assertEquals(true, map.at(999998));
    assertEquals(false, map.at(999999));
    assertEquals(false, map.at(1000000));
  },
});
