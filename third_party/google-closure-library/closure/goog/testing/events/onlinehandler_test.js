/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.events.OnlineHandlerTest');
goog.setTestOnly();

const EventObserver = goog.require('goog.testing.events.EventObserver');
const NetworkStatusMonitor = goog.require('goog.net.NetworkStatusMonitor');
const OnlineHandler = goog.require('goog.testing.events.OnlineHandler');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

let handler;

let observer;

function createHandler(initialValue) {
  handler = new OnlineHandler(initialValue);
  observer = new EventObserver();
  events.listen(
      handler,
      [
        NetworkStatusMonitor.EventType.ONLINE,
        NetworkStatusMonitor.EventType.OFFLINE,
      ],
      observer);
}

function assertEventCounts(expectedOnlineEvents, expectedOfflineEvents) {
  assertEquals(
      expectedOnlineEvents,
      observer.getEvents(NetworkStatusMonitor.EventType.ONLINE).length);
  assertEquals(
      expectedOfflineEvents,
      observer.getEvents(NetworkStatusMonitor.EventType.OFFLINE).length);
}
testSuite({
  tearDown() {
    handler = null;
    observer = null;
  },

  testInitialValue() {
    createHandler(true);
    assertEquals(true, handler.isOnline());
    createHandler(false);
    assertEquals(false, handler.isOnline());
  },

  testStateChange() {
    createHandler(true);
    assertEventCounts(
        0 /* expectedOnlineEvents */, 0 /* expectedOfflineEvents */);

    // Expect no events.
    handler.setOnline(true);
    assertEquals(true, handler.isOnline());
    assertEventCounts(
        0 /* expectedOnlineEvents */, 0 /* expectedOfflineEvents */);

    // Expect one offline event.
    handler.setOnline(false);
    assertEquals(false, handler.isOnline());
    assertEventCounts(
        0 /* expectedOnlineEvents */, 1 /* expectedOfflineEvents */);

    // Expect no events.
    handler.setOnline(false);
    assertEquals(false, handler.isOnline());
    assertEventCounts(
        0 /* expectedOnlineEvents */, 1 /* expectedOfflineEvents */);

    // Expect one online event.
    handler.setOnline(true);
    assertEquals(true, handler.isOnline());
    assertEventCounts(
        1 /* expectedOnlineEvents */, 1 /* expectedOfflineEvents */);
  },
});
