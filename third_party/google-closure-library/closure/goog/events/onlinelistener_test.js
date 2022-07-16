/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.events.OnlineHandlerTest');
goog.setTestOnly();

const GoogEvent = goog.require('goog.events.Event');
const MockClock = goog.require('goog.testing.MockClock');
const NetworkStatusMonitor = goog.require('goog.net.NetworkStatusMonitor');
const OnlineHandler = goog.require('goog.events.OnlineHandler');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

const stubs = new PropertyReplacer();
const clock = new MockClock();
let online = true;
let onlineCount;
let offlineCount;

function listenToEvents(oh) {
  onlineCount = 0;
  offlineCount = 0;

  events.listen(oh, NetworkStatusMonitor.EventType.ONLINE, (e) => {
    assertTrue(oh.isOnline());
    onlineCount++;
  });
  events.listen(oh, NetworkStatusMonitor.EventType.OFFLINE, (e) => {
    assertFalse(oh.isOnline());
    offlineCount++;
  });
}

testSuite({
  setUp() {
    stubs.set(OnlineHandler.prototype, 'isOnline', () => online);
  },

  tearDown() {
    stubs.reset();
    clock.uninstall();
  },

  testConstructAndDispose() {
    const oh = new OnlineHandler();
    oh.dispose();
  },


  testHtml5() {
    // Test for browsers that fire network events on window.

    let oh = new OnlineHandler();
    listenToEvents(oh);

    online = false;
    let e = new GoogEvent('offline');
    events.fireListeners(window, e.type, false, e);

    assertEquals(0, onlineCount);
    assertEquals(1, offlineCount);

    online = true;
    e = new GoogEvent('online');
    events.fireListeners(window, e.type, false, e);

    assertEquals(1, onlineCount);
    assertEquals(1, offlineCount);

    oh.dispose();
  },
});
