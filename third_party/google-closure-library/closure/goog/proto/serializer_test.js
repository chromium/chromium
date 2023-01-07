/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.protoTest');
goog.setTestOnly();

const proto = goog.require('goog.proto');
const testSuite = goog.require('goog.testing.testSuite');

const serialize = proto.serialize;

/**
 * Returns an array with the given elements and length.
 * @param {!Array<T>} elems The elements in the array.
 * @param {number} length The length.
 * @return {!Array<T>} The original 'elems' array with its length changed.
 * @template T
 */
function withLength(elems, length) {
  elems.length = length;
  return elems;
}
testSuite({
  testArraySerialize() {
    assertEquals('Empty array', serialize([]), '[]');

    assertEquals('Normal array', serialize([0, 1, 2]), '[0,1,2]');
    assertEquals('Empty start', serialize([, 1, 2]), '[null,1,2]');
    assertEquals(
        'Empty start', serialize([, , , 3, 4]), '[null,null,null,3,4]');
    assertEquals('Empty middle', serialize([0, , 2]), '[0,null,2]');
    assertEquals('Empty middle', serialize([0, , , 3]), '[0,null,null,3]');
    assertEquals('Empty end', serialize(withLength([0, 1, 2], 4)), '[0,1,2]');
    assertEquals(
        'Empty start, middle and end', serialize([, , 2, , 4, null]),
        '[null,null,2,null,4]');
    assertEquals('All elements empty', serialize(withLength([], 3)), '[]');
    assertEquals(
        'Nested', serialize([, 1, [, 1, [, 1]]]), '[null,1,[null,1,[null,1]]]');
  },
});
