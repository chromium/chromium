/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.PseudoRandomTest');
goog.setTestOnly();

const PseudoRandom = goog.require('goog.testing.PseudoRandom');
const testSuite = goog.require('goog.testing.testSuite');

function runFairnessTest(sides, rolls, chiSquareLimit) {
  // Initialize the count table for dice rolls.
  const counts = [];
  for (let i = 0; i < sides; ++i) {
    counts[i] = 0;
  }
  // Roll the dice many times and count the results.
  for (let i = 0; i < rolls; ++i) {
    ++counts[Math.floor(Math.random() * sides)];
  }
  // If the dice is fair, we expect a uniform distribution.
  const expected = rolls / sides;
  // Pearson's chi-square test for a distribution fit.
  let chiSquare = 0;
  for (let i = 0; i < sides; ++i) {
    chiSquare += (counts[i] - expected) * (counts[i] - expected) / expected;
  }
  assert(
      'Chi-square test for a distribution fit failed',
      chiSquare < chiSquareLimit);
}

testSuite({
  /** @suppress {visibility} suppression added to enable type checking */
  testInstall() {
    const random = new PseudoRandom();
    const originalRandom = Math.random;

    assertFalse(!!random.installed_);

    random.install();
    assertTrue(random.installed_);
    assertNotEquals(Math.random, originalRandom);

    random.uninstall();
    assertFalse(random.installed_);
    assertEquals(originalRandom, Math.random);
  },

  testBounds() {
    const random = new PseudoRandom();
    random.install();

    for (let i = 0; i < 100000; ++i) {
      const value = Math.random();
      assert('Random value out of bounds', value >= 0 && value < 1);
    }

    random.uninstall();
  },

  testFairness() {
    const random = new PseudoRandom(0, true);

    // Chi-square statistics: p-value = 0.05, df = 5, limit = 11.07.
    runFairnessTest(6, 100000, 11.07);
    // Chi-square statistics: p-value = 0.05, df = 100, limit = 124.34.
    runFairnessTest(101, 100000, 124.34);

    random.uninstall();
  },

  testReseed() {
    const random = new PseudoRandom(100, true);

    const sequence = [];
    for (let i = 0; i < 64000; ++i) {
      sequence.push(Math.random());
    }

    random.seed(100);
    for (let i = 0; i < sequence.length; ++i) {
      assertEquals(sequence[i], Math.random());
    }

    random.uninstall();
  },
});
