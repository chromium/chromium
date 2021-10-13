/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.graphics.SolidFillTest');
goog.setTestOnly();

const SolidFill = goog.require('goog.graphics.SolidFill');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testGetColor() {
    let fill = new SolidFill('#123');
    assertEquals('#123', fill.getColor());
    fill = new SolidFill('#abcdef');
    assertEquals('#abcdef', fill.getColor());

    fill = new SolidFill('#123', 0.5);
    assertEquals('#123', fill.getColor());
    fill = new SolidFill('#abcdef', 0.5);
    assertEquals('#abcdef', fill.getColor());
  },

  testGetOpacity() {
    // Default opacity
    let fill = new SolidFill('#123');
    assertEquals(1, fill.getOpacity());

    // Opaque
    fill = new SolidFill('#123', 1);
    assertEquals(1, fill.getOpacity());

    // Semi-transparent
    fill = new SolidFill('#123', 0.5);
    assertEquals(0.5, fill.getOpacity());

    // Fully transparent
    fill = new SolidFill('#123', 0);
    assertEquals(0, fill.getOpacity());
  },
});
