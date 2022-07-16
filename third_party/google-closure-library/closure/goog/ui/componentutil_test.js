/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ComponentUtilTest');
goog.setTestOnly();

const Component = goog.require('goog.ui.Component');
const ComponentUtil = goog.require('goog.ui.ComponentUtil');
const MouseAsMouseEventType = goog.require('goog.events.MouseAsMouseEventType');
const PointerAsMouseEventType = goog.require('goog.events.PointerAsMouseEventType');
const testSuite = goog.require('goog.testing.testSuite');

let component;

testSuite({
  setUp() {
    component = new Component();
  },

  tearDown() {
    component.dispose();
  },

  testGetMouseEventType() {
    component.setPointerEventsEnabled(false);
    assertEquals(
        'Component must use mouse events when specified.',
        ComponentUtil.getMouseEventType(component), MouseAsMouseEventType);

    component.setPointerEventsEnabled(true);
    assertEquals(
        'Component must use pointer events when specified.',
        ComponentUtil.getMouseEventType(component), PointerAsMouseEventType);
  },
});
