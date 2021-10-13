/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.events.NonDisposableEventTargetGoogEventsTest');
goog.setTestOnly();

const NonDisposableEventTarget = goog.require('goog.labs.events.NonDisposableEventTarget');
const eventTargetTester = goog.require('goog.events.eventTargetTester');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');
const testing = goog.require('goog.testing');

const KeyType = eventTargetTester.KeyType;
const EventType = eventTargetTester.EventType;
const UnlistenReturnType = eventTargetTester.UnlistenReturnType;

testSuite({
  setUp() {
    const newListenableFn = () => new NonDisposableEventTarget();
    const unlistenByKeyFn = (src, key) => events.unlistenByKey(key);
    eventTargetTester.setUp(
        newListenableFn, events.listen, events.unlisten, unlistenByKeyFn,
        events.listenOnce, events.dispatchEvent, events.removeAll,
        events.getListeners, events.getListener, events.hasListener,
        KeyType.NUMBER, UnlistenReturnType.BOOLEAN, true);
  },

  tearDown() {
    eventTargetTester.tearDown();
  },

  testUnlistenProperCleanup() {
    events.listen(
        eventTargetTester.getTargets()[0], EventType.A,
        eventTargetTester.getListeners()[0]);
    events.unlisten(
        eventTargetTester.getTargets()[0], EventType.A,
        eventTargetTester.getListeners()[0]);

    events.listen(
        eventTargetTester.getTargets()[0], EventType.A,
        eventTargetTester.getListeners()[0]);
    eventTargetTester.getTargets()[0].unlisten(
        EventType.A, eventTargetTester.getListeners()[0]);
  },

  testUnlistenByKeyProperCleanup() {
    const keyNum = events.listen(
        eventTargetTester.getTargets()[0], EventType.A,
        eventTargetTester.getListeners()[0]);
    events.unlistenByKey(keyNum);
  },

  testListenOnceProperCleanup() {
    events.listenOnce(
        eventTargetTester.getTargets()[0], EventType.A,
        eventTargetTester.getListeners()[0]);
    eventTargetTester.getTargets()[0].dispatchEvent(EventType.A);
  },

  testListenWithObject() {
    const obj = {};
    obj.handleEvent = testing.recordFunction();
    events.listen(eventTargetTester.getTargets()[0], EventType.A, obj);
    eventTargetTester.getTargets()[0].dispatchEvent(EventType.A);
    assertEquals(1, obj.handleEvent.getCallCount());
  },

  testListenWithObjectHandleEventReturningFalse() {
    const obj = {};
    obj.handleEvent = () => false;
    events.listen(eventTargetTester.getTargets()[0], EventType.A, obj);
    assertFalse(eventTargetTester.getTargets()[0].dispatchEvent(EventType.A));
  },
});
