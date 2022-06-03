/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Tests for {@link goog.html.sanitizer.ElementWeakMap} */

goog.module('goog.html.sanitizer.ElementWeakMapTest');
goog.setTestOnly();

const ElementWeakMap = goog.require('goog.html.sanitizer.ElementWeakMap');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

/** @const {boolean} */
const ELEMENTWEAKMAP_SUPPORTED = !userAgent.IE || document.documentMode >= 10;

testSuite({
  testBasic() {
    if (!ELEMENTWEAKMAP_SUPPORTED) {
      return;
    }
    const el1 = document.createElement('a');
    const el2 = document.createElement('b');
    const el3 = document.createElement('a');
    const weakMap = ElementWeakMap.newWeakMap();
    weakMap.set(el1, 1);
    weakMap.set(el2, 2);

    assertEquals(1, weakMap.get(el1));
    assertEquals(2, weakMap.get(el2));
    assertUndefined(weakMap.get(el3));
  },

  testDuplicates() {
    if (!ELEMENTWEAKMAP_SUPPORTED) {
      return;
    }
    const el1 = document.createElement('a');
    const el2 = document.createElement('a');
    const weakMap = ElementWeakMap.newWeakMap();
    weakMap.set(el1, 1);
    weakMap.set(el1, 2);

    assertEquals(2, weakMap.get(el1));
    assertUndefined(weakMap.get(el2));
  },

  testClear() {
    if (!ELEMENTWEAKMAP_SUPPORTED) {
      return;
    }
    const el = document.createElement('a');
    const weakMap = ElementWeakMap.newWeakMap();
    weakMap.set(el, 1);
    weakMap.set(el, 2);

    if (weakMap.clear) {
      weakMap.clear();
    }
  }
});
