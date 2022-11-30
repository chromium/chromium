/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.EventTypeTest');
goog.setTestOnly();

const BrowserFeature = goog.require('goog.events.BrowserFeature');
const EventType = goog.require('goog.events.EventType');
const PointerFallbackEventType = goog.require('goog.events.PointerFallbackEventType');
const PointerTouchFallbackEventType = goog.require('goog.events.PointerTouchFallbackEventType');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

testSuite({
  testPointerFallbackEventType() {
    if (BrowserFeature.POINTER_EVENTS) {
      // Pointer events are supported; use W3C PointerEvent
      assertEquals(EventType.POINTERDOWN, PointerFallbackEventType.POINTERDOWN);
    } else if (BrowserFeature.MSPOINTER_EVENTS) {
      // Only IE10 should support MSPointerEvent
      assertTrue(
          userAgent.IE && userAgent.isVersionOrHigher('10') &&
          !userAgent.isVersionOrHigher('11'));
      // W3C PointerEvent not supported; fall back to MSPointerEvent
      assertEquals(
          EventType.MSPOINTERDOWN, PointerFallbackEventType.POINTERDOWN);
    } else {
      // Pointer events not supported; fall back to MouseEvent
      assertEquals(EventType.MOUSEDOWN, PointerFallbackEventType.POINTERDOWN);
    }
  },

  testPointerTouchFallbackEventType() {
    if (BrowserFeature.POINTER_EVENTS) {
      // Pointer events are supported; use W3C PointerEvent
      assertEquals(
          EventType.POINTERDOWN, PointerTouchFallbackEventType.POINTERDOWN);
    } else if (BrowserFeature.MSPOINTER_EVENTS) {
      // Only IE10 should support MSPointerEvent
      assertTrue(
          userAgent.IE && userAgent.isVersionOrHigher('10') &&
          !userAgent.isVersionOrHigher('11'));
      // W3C PointerEvent not supported; fall back to MSPointerEvent
      assertEquals(
          EventType.MSPOINTERDOWN, PointerTouchFallbackEventType.POINTERDOWN);
    } else {
      // Pointer events not supported; fall back to TouchEvent
      assertEquals(
          EventType.TOUCHSTART, PointerTouchFallbackEventType.POINTERDOWN);
    }
  },
});
