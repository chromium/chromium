/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for touch. */

goog.module('goog.labs.events.touchTest');
goog.setTestOnly();

const testSuite = goog.require('goog.testing.testSuite');
const touch = goog.require('goog.labs.events.touch');

testSuite({
  testMouseEvent() {
    const fakeTarget = {};

    const fakeMouseMove = {
      'clientX': 1,
      'clientY': 2,
      'screenX': 3,
      'screenY': 4,
      'target': fakeTarget,
      'type': 'mousemove',
    };

    /** @suppress {checkTypes} suppression added to enable type checking */
    const data = touch.getTouchData(fakeMouseMove);
    assertEquals(1, data.clientX);
    assertEquals(2, data.clientY);
    assertEquals(3, data.screenX);
    assertEquals(4, data.screenY);
    assertEquals(fakeTarget, data.target);
  },

  testTouchEvent() {
    const fakeTarget = {};

    const fakeTouch = {
      'clientX': 1,
      'clientY': 2,
      'screenX': 3,
      'screenY': 4,
      'target': fakeTarget,
    };

    const fakeTouchStart = {
      'targetTouches': [fakeTouch],
      'target': fakeTarget,
      'type': 'touchstart',
    };

    /** @suppress {checkTypes} suppression added to enable type checking */
    const data = touch.getTouchData(fakeTouchStart);
    assertEquals(1, data.clientX);
    assertEquals(2, data.clientY);
    assertEquals(3, data.screenX);
    assertEquals(4, data.screenY);
    assertEquals(fakeTarget, data.target);
  },

  testTouchChangeEvent() {
    const fakeTarget = {};

    const fakeTouch = {
      'clientX': 1,
      'clientY': 2,
      'screenX': 3,
      'screenY': 4,
      'target': fakeTarget,
    };

    const fakeTouchStart = {
      'changedTouches': [fakeTouch],
      'target': fakeTarget,
      'type': 'touchend'
    };

    /** @suppress {checkTypes} suppression added to enable type checking */
    const data = touch.getTouchData(fakeTouchStart);
    assertEquals(1, data.clientX);
    assertEquals(2, data.clientY);
    assertEquals(3, data.screenX);
    assertEquals(4, data.screenY);
    assertEquals(fakeTarget, data.target);
  },
});
