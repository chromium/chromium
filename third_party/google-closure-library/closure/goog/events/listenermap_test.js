/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Tests for listenermap.js.
 * Most of this class functionality is already tested by
 * GoogEventTarget tests. This test file only provides tests
 * for features that are not direct duplicates of tests in
 * GoogEventTarget.
 */

goog.module('goog.events.ListenerMapTest');
goog.setTestOnly();

const EventId = goog.require('goog.events.EventId');
const GoogEventTarget = goog.require('goog.events.EventTarget');
const ListenerMap = goog.require('goog.events.ListenerMap');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

let et;
let map;

const handler1 = () => {};
const handler2 = () => {};
const handler3 = () => {};
const CLICK_EVENT_ID = new EventId(events.getUniqueId('click'));

testSuite({
  setUp() {
    et = new GoogEventTarget();
    map = new ListenerMap(et);
  },

  tearDown() {
    dispose(et);
  },

  testGetTypeCount() {
    assertEquals(0, map.getTypeCount());

    map.add('click', handler1, false);
    assertEquals(1, map.getTypeCount());
    map.remove('click', handler1);
    assertEquals(0, map.getTypeCount());

    map.add(CLICK_EVENT_ID, handler1, false);
    assertEquals(1, map.getTypeCount());
    map.remove(CLICK_EVENT_ID, handler1);
    assertEquals(0, map.getTypeCount());

    map.add('click', handler1, false, true);
    assertEquals(1, map.getTypeCount());
    map.remove('click', handler1, true);
    assertEquals(0, map.getTypeCount());

    map.add(CLICK_EVENT_ID, handler1, false, true);
    assertEquals(1, map.getTypeCount());
    map.remove(CLICK_EVENT_ID, handler1, true);
    assertEquals(0, map.getTypeCount());

    map.add('click', handler1, false);
    map.add('click', handler1, false, true);
    assertEquals(1, map.getTypeCount());
    map.remove('click', handler1);
    assertEquals(1, map.getTypeCount());
    map.remove('click', handler1, true);
    assertEquals(0, map.getTypeCount());

    map.add(CLICK_EVENT_ID, handler1, false);
    map.add(CLICK_EVENT_ID, handler1, false, true);
    assertEquals(1, map.getTypeCount());
    map.remove(CLICK_EVENT_ID, handler1);
    assertEquals(1, map.getTypeCount());
    map.remove(CLICK_EVENT_ID, handler1, true);
    assertEquals(0, map.getTypeCount());

    map.add('click', handler1, false);
    map.add('touchstart', handler2, false);
    map.add(CLICK_EVENT_ID, handler3, false);
    assertEquals(3, map.getTypeCount());
    map.remove(CLICK_EVENT_ID, handler3);
    assertEquals(2, map.getTypeCount());
    map.remove('touchstart', handler2);
    assertEquals(1, map.getTypeCount());
    map.remove('click', handler1);
    assertEquals(0, map.getTypeCount());
  },

  testGetListenerCount() {
    assertEquals(0, map.getListenerCount());

    map.add('click', handler1, false);
    assertEquals(1, map.getListenerCount());
    map.remove('click', handler1);
    assertEquals(0, map.getListenerCount());

    map.add(CLICK_EVENT_ID, handler1, false);
    assertEquals(1, map.getListenerCount());
    map.remove(CLICK_EVENT_ID, handler1);
    assertEquals(0, map.getListenerCount());

    map.add('click', handler1, false, true);
    assertEquals(1, map.getListenerCount());
    map.remove('click', handler1, true);
    assertEquals(0, map.getListenerCount());

    map.add(CLICK_EVENT_ID, handler1, false, true);
    assertEquals(1, map.getListenerCount());
    map.remove(CLICK_EVENT_ID, handler1, true);
    assertEquals(0, map.getListenerCount());

    map.add('click', handler1, false);
    map.add('click', handler1, false, true);
    assertEquals(2, map.getListenerCount());
    map.remove('click', handler1);
    map.remove('click', handler1, true);
    assertEquals(0, map.getListenerCount());

    map.add(CLICK_EVENT_ID, handler1, false);
    map.add(CLICK_EVENT_ID, handler1, false, true);
    assertEquals(2, map.getListenerCount());
    map.remove(CLICK_EVENT_ID, handler1);
    map.remove(CLICK_EVENT_ID, handler1, true);
    assertEquals(0, map.getListenerCount());

    map.add('click', handler1, false);
    map.add('touchstart', handler2, false);
    map.add(CLICK_EVENT_ID, handler3, false);
    assertEquals(3, map.getListenerCount());
    map.remove(CLICK_EVENT_ID, handler3);
    map.remove('touchstart', handler2);
    map.remove('click', handler1);
    assertEquals(0, map.getListenerCount());
  },

  testListenerSourceIsSetCorrectly() {
    map.add('click', handler1, false);
    let listener = map.getListener('click', handler1);
    assertEquals(et, listener.src);

    map.add(CLICK_EVENT_ID, handler2, false);
    listener = map.getListener(CLICK_EVENT_ID, handler2);
    assertEquals(et, listener.src);
  },
});
