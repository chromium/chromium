// Copyright 2014 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Tests for goog.labs.iterable
 */

goog.module('goog.labs.iterableTest');
goog.setTestOnly('goog.labs.iterableTest');

const iterables = goog.require('goog.labs.collections.iterables');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');


/**
 * Create an iterator starting at "start" and increments up to
 * (but not including) "stop".
 */
function createRangeIterator(start, stop) {
  let value = start;
  const next = () => {
    if (value < stop) {
      return {value: value++, done: false};
    }

    return {value: undefined, done: true};
  };

  return {next: next};
}

function createRangeIterable(start, stop) {
  const obj = {};

  // Refer to goog.global['Symbol'] because otherwise this
  // is a parse error in earlier IEs.
  obj[goog.global['Symbol'].iterator] = () => createRangeIterator(start, stop);
  return obj;
}

function isSymbolDefined() {
  return !!goog.global['Symbol'];
}

testSuite({
  testCreateRangeIterable() {
    // Do not run if Symbol does not exist in this browser.
    if (!isSymbolDefined()) {
      return;
    }

    const rangeIterator = createRangeIterator(0, 3);

    for (let i = 0; i < 3; i++) {
      assertObjectEquals({value: i, done: false}, rangeIterator.next());
    }

    for (let i = 0; i < 3; i++) {
      assertObjectEquals({value: undefined, done: true}, rangeIterator.next());
    }
  },

  testForEach() {
    // Do not run if Symbol does not exist in this browser.
    if (!isSymbolDefined()) {
      return;
    }

    const range = createRangeIterable(0, 3);

    const callback = recordFunction();
    iterables.forEach(range, callback);

    callback.assertCallCount(3);

    const calls = callback.getCalls();
    for (let i = 0; i < calls.length; i++) {
      const call = calls[i];
      assertArrayEquals([i], call.getArguments());
    }
  },

  testMap() {
    // Do not run if Symbol does not exist in this browser.
    if (!isSymbolDefined()) {
      return;
    }

    const range = createRangeIterable(0, 3);

    function addTwo(i) {
      return i + 2;
    }

    const newIterable = iterables.map(range, addTwo);
    const newIterator = iterables.getIterator(newIterable);

    let nextObj = newIterator.next();
    assertEquals(2, nextObj.value);
    assertFalse(nextObj.done);

    nextObj = newIterator.next();
    assertEquals(3, nextObj.value);
    assertFalse(nextObj.done);

    nextObj = newIterator.next();
    assertEquals(4, nextObj.value);
    assertFalse(nextObj.done);

    // Check that the iterator repeatedly signals done.
    for (let i = 0; i < 3; i++) {
      nextObj = newIterator.next();
      assertUndefined(nextObj.value);
      assertTrue(nextObj.done);
    }
  },

  testFilter() {
    function isEven(val) {
      return val % 2 == 0;
    }

    const range = createRangeIterable(0, 6);
    const newIterable = iterables.filter(range, isEven);
    const newIterator = iterables.getIterator(newIterable);

    let nextObj = newIterator.next();
    assertEquals(0, nextObj.value);
    assertFalse(nextObj.done);

    nextObj = newIterator.next();
    assertEquals(2, nextObj.value);
    assertFalse(nextObj.done);

    nextObj = newIterator.next();
    assertEquals(4, nextObj.value);
    assertFalse(nextObj.done);

    // Check that the iterator repeatedly signals done.
    for (let i = 0; i < 3; i++) {
      nextObj = newIterator.next();
      assertUndefined(nextObj.value);
      assertTrue(nextObj.done);
    }
  }
});
