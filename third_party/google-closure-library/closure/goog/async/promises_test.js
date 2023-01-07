/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.async.promisesTest');
goog.setTestOnly();

const promises = goog.require('goog.async.promises');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  async testAllMapValues_resolve() {
    const /** !Map<string, !Promise<number>> */ promiseMap = new Map([
      ['a', Promise.resolve(1)],
      ['b', Promise.resolve(2)],
      ['c', Promise.resolve(3)],
    ]);

    const expectedEntries = [
      ['a', 1],
      ['b', 2],
      ['c', 3],
    ];

    const /** !Map<string, number> */ resultMap =
        await promises.allMapValues(promiseMap);
    assertArrayEquals(expectedEntries, Array.from(resultMap.entries()));
  },

  async testAllMapValues_reject() {
    const testError = new Error('test rejected error');
    const promiseMap /** !Map<string, !Promise<number>> */ = new Map([
      ['a', Promise.resolve(1)],
      ['b', Promise.reject(testError)],
      ['c', Promise.resolve(3)],
    ]);

    const e = await assertRejects(promises.allMapValues(promiseMap));
    assertEquals(testError, e);
  },

  async testAllMapValues_empty() {
    const /** !Map<string, !Promise<number>> */ promiseMap = new Map();

    const expectedEntries = [];

    const /** !Map<string, number> */ resultMap =
        await promises.allMapValues(promiseMap);
    assertArrayEquals(expectedEntries, Array.from(resultMap.entries()));
  },

  async testAllMapValues_nonThenableValues() {
    const promiseMap /** !Map<string, number> */ = new Map([
      ['a', 1],
      ['b', 2],
      ['c', 3],
    ]);

    const expectedEntries = [
      ['a', 1],
      ['b', 2],
      ['c', 3],
    ];

    const /** !Map<string, number> */ resultMap =
        await promises.allMapValues(promiseMap);
    assertArrayEquals(expectedEntries, Array.from(resultMap.entries()));
  },

  async testAllMapValues_IThenableValues() {
    /** @implements {IThenable<undefined>} */
    class TestIThenable {
      /** @override */
      then(onFulfilled, onRejected) {
        onFulfilled(undefined);
        return this;
      }
    }

    const /** !Map<string, !TestIThenable> */ promiseMap = new Map([
      ['a', new TestIThenable()],
    ]);

    const expectedEntries = [
      ['a', undefined],
    ];

    const /** !Map<string, undefined> */ resultMap =
        await promises.allMapValues(promiseMap);
    assertArrayEquals(expectedEntries, Array.from(resultMap.entries()));
  },
});
