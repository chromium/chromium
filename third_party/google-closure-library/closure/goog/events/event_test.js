/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.EventTest');
goog.setTestOnly();

const EventId = goog.require('goog.events.EventId');
const GoogEvent = goog.require('goog.events.Event');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const testSuite = goog.require('goog.testing.testSuite');

let e;
let target;

testSuite({
  setUp() {
    target = new GoogEventTarget();
    e = new GoogEvent('eventType', target);
  },

  tearDown() {
    target.dispose();
  },

  testConstructor() {
    assertNotNull('Event must not be null', e);
    assertEquals('Event type must be as expected', 'eventType', e.type);
    assertEquals('Event target must be as expected', target, e.target);
    assertEquals('Current target must be as expected', target, e.currentTarget);
  },

  testStopPropagation() {
    assertFalse(
        'Propagation must not have been stopped', e.hasPropagationStopped());
    e.stopPropagation();
    assertTrue('Propagation must have been stopped', e.hasPropagationStopped());
  },

  testDefaultPrevented() {
    assertFalse('Default action must not be prevented', e.defaultPrevented);
    e.preventDefault();
    assertTrue('Default action must be prevented', e.defaultPrevented);
  },

  testEventId() {
    e = new GoogEvent(new EventId('eventType'));
    assertEquals('Event type must be as expected', 'eventType', e.type);
  },
});
