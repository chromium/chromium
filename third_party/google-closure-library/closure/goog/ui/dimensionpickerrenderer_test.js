/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.DimensionPickerRendererTest');
goog.setTestOnly();

const DimensionPicker = goog.require('goog.ui.DimensionPicker');
const DimensionPickerRenderer = goog.require('goog.ui.DimensionPickerRenderer');
const LivePriority = goog.require('goog.a11y.aria.LivePriority');
const googArray = goog.require('goog.array');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let renderer;
let picker;

testSuite({
  setUp() {
    renderer = new DimensionPickerRenderer();
    picker = new DimensionPicker(renderer);
  },

  tearDown() {
    picker.dispose();
  },

  /**
   * Tests that the right aria label is added when the highlighted
   * size changes.
   */
  testSetHighlightedSizeUpdatesLiveRegion() {
    picker.render();

    const sayFunction = recordFunction();
    /** @suppress {visibility} suppression added to enable type checking */
    renderer.announcer_.say = sayFunction;
    renderer.setHighlightedSize(picker, 3, 7);

    assertEquals(1, sayFunction.getCallCount());

    assertTrue(googArray.equals(
        ['3 by 7', LivePriority.ASSERTIVE],
        sayFunction.getLastCall().getArguments()));
  },
});
