/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.MockRandomTest');
goog.setTestOnly();

const MockRandom = goog.require('goog.testing.MockRandom');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testMockRandomInstall() {
    const random = new MockRandom([]);
    const originalRandom = Math.random;

    assertFalse(!!random.installed_);

    random.install();
    assertTrue(random.installed_);
    assertNotEquals(Math.random, originalRandom);

    random.uninstall();
    assertFalse(random.installed_);
    assertEquals(originalRandom, Math.random);
  },

  testMockRandomRandom() {
    const random = new MockRandom([], true);

    assertFalse(random.hasMoreValues());

    random.inject(2);
    assertTrue(random.hasMoreValues());
    assertEquals(2, Math.random());

    random.inject([1, 2, 3]);
    assertTrue(random.hasMoreValues());
    assertEquals(1, Math.random());
    assertEquals(2, Math.random());
    assertEquals(3, Math.random());
    assertFalse(random.hasMoreValues());
    assertNotUndefined(Math.random());
  },

  testRandomStrictlyFromSequence() {
    const random = new MockRandom([], /* install */ true);
    random.setStrictlyFromSequence(true);
    assertFalse(random.hasMoreValues());
    assertThrows(/**
                    @suppress {uselessCode} suppression added to enable type
                    checking
                  */
                 () => {
                   Math.random();
                 });

    random.inject(3);
    assertTrue(random.hasMoreValues());
    assertNotThrows(/**
                       @suppress {uselessCode} suppression added to enable type
                       checking
                     */
                    () => {
                      Math.random();
                    });

    random.setStrictlyFromSequence(false);
    assertFalse(random.hasMoreValues());
    assertNotThrows(/**
                       @suppress {uselessCode} suppression added to enable type
                       checking
                     */
                    () => {
                      Math.random();
                    });
  },
});
