/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.JsonFuzzingTest');
goog.setTestOnly();

const JsonFuzzing = goog.require('goog.labs.testing.JsonFuzzing');
const asserts = goog.require('goog.testing.asserts');
const googJson = goog.require('goog.json');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testValidJson() {
    const fuzzing = new JsonFuzzing();  // seeded with now()

    for (let i = 0; i < 10; i++) {
      const data = fuzzing.newArray();
      assertTrue(Array.isArray(data));
      // JSON compatible
      assertNotThrows(() => {
        googJson.serialize(data);
      });
    }
  },
});
