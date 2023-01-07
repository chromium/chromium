/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.ActivityMonitorTest');
goog.setTestOnly();

const ActivityMonitor = goog.require('goog.ui.ActivityMonitor');
const EventType = goog.require('goog.events.EventType');
const GoogEvent = goog.require('goog.events.Event');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

let mockClock;
const stubs = new PropertyReplacer();
let mydiv;

testSuite({
  setUp() {
    mydiv = document.getElementById('mydiv');
    mockClock = new MockClock(true);
    // ActivityMonitor initializes a private to 0 which it compares to now(),
    // so tests fail unless we start the mock clock with something besides 0.
    mockClock.tick(1);
  },

  tearDown() {
    mockClock.dispose();
    stubs.reset();
  },

  testIdle() {
    const activityMonitor = new ActivityMonitor();
    assertEquals(
        'Upon creation, last event time should be creation time',
        mockClock.getCurrentTime(), activityMonitor.getLastEventTime());

    mockClock.tick(1000);
    activityMonitor.resetTimer();
    const resetTime = mockClock.getCurrentTime();
    assertEquals(
        'Upon reset, last event time should be reset time', resetTime,
        activityMonitor.getLastEventTime());
    assertEquals(
        'Upon reset, idle time should be zero', 0,
        activityMonitor.getIdleTime());

    mockClock.tick(1000);
    assertEquals(
        '1s after reset, last event time should be reset time', resetTime,
        activityMonitor.getLastEventTime());
    assertEquals(
        '1s after reset, idle time should be 1s', 1000,
        activityMonitor.getIdleTime());
  },

  testEventFired() {
    const activityMonitor = new ActivityMonitor();
    const listener = recordFunction();
    events.listen(activityMonitor, ActivityMonitor.Event.ACTIVITY, listener);

    mockClock.tick(1000);
    testingEvents.fireClickEvent(mydiv);
    assertEquals(
        'Activity event should fire when click happens after creation', 1,
        listener.getCallCount());

    mockClock.tick(3000);
    testingEvents.fireClickEvent(mydiv);
    assertEquals(
        'Activity event should not fire when click happens 3s or ' +
            'less since the last activity',
        1, listener.getCallCount());

    mockClock.tick(1);
    testingEvents.fireClickEvent(mydiv);
    assertEquals(
        'Activity event should fire when click happens more than ' +
            '3s since the last activity',
        2, listener.getCallCount());
  },

  testEventFiredWhenPropagationStopped() {
    const activityMonitor = new ActivityMonitor();
    const listener = recordFunction();
    events.listen(activityMonitor, ActivityMonitor.Event.ACTIVITY, listener);

    events.listenOnce(mydiv, EventType.CLICK, GoogEvent.stopPropagation);
    testingEvents.fireClickEvent(mydiv);
    assertEquals(
        'Activity event should fire despite click propagation ' +
            'stopped because listening on capture',
        1, listener.getCallCount());
  },

  testEventNotFiredWhenPropagationStopped() {
    const activityMonitor = new ActivityMonitor(undefined, true);
    const listener = recordFunction();
    events.listen(activityMonitor, ActivityMonitor.Event.ACTIVITY, listener);

    events.listenOnce(mydiv, EventType.CLICK, GoogEvent.stopPropagation);
    testingEvents.fireClickEvent(mydiv);
    assertEquals(
        'Activity event should not fire since click propagation ' +
            'stopped and listening on bubble',
        0, listener.getCallCount());
  },

  testTouchSequenceFired() {
    const activityMonitor = new ActivityMonitor();
    const listener = recordFunction();
    events.listen(activityMonitor, ActivityMonitor.Event.ACTIVITY, listener);

    mockClock.tick(1000);
    testingEvents.fireTouchSequence(mydiv);
    assertEquals(
        'Activity event should fire when touch happens after creation', 1,
        listener.getCallCount());

    mockClock.tick(3000);
    testingEvents.fireTouchSequence(mydiv);
    assertEquals(
        'Activity event should not fire when touch happens 3s or ' +
            'less since the last activity',
        1, listener.getCallCount());

    mockClock.tick(1);
    testingEvents.fireTouchSequence(mydiv);
    assertEquals(
        'Activity event should fire when touch happens more than ' +
            '3s since the last activity',
        2, listener.getCallCount());
  },

  /** @suppress {visibility} suppression added to enable type checking */
  testAddDocument_duplicate() {
    const defaultDoc = dom.getDomHelper().getDocument();
    const activityMonitor = new ActivityMonitor();
    assertEquals(1, activityMonitor.documents_.length);
    assertEquals(defaultDoc, activityMonitor.documents_[0]);
    /** @suppress {visibility} suppression added to enable type checking */
    const listenerCount = activityMonitor.eventHandler_.getListenerCount();

    activityMonitor.addDocument(defaultDoc);
    assertEquals(1, activityMonitor.documents_.length);
    assertEquals(defaultDoc, activityMonitor.documents_[0]);
    assertEquals(
        listenerCount, activityMonitor.eventHandler_.getListenerCount());
  },
});
