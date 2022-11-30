/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.dom.PageVisibilityMonitorTest');
goog.setTestOnly();

const GoogTestingEvent = goog.require('goog.testing.events.Event');
const PageVisibilityMonitor = goog.require('goog.labs.dom.PageVisibilityMonitor');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const functions = goog.require('goog.functions');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');
const testingEvents = goog.require('goog.testing.events');

const stubs = new PropertyReplacer();
let vh;

testSuite({
  tearDown() {
    dispose(vh);
    vh = null;
    stubs.reset();
  },

  testConstructor() {
    vh = new PageVisibilityMonitor();
  },

  /** @suppress {const} See go/const-js-library-faq */
  testNoVisibilitySupport() {
    stubs.set(
        PageVisibilityMonitor.prototype, 'getBrowserEventType_',
        functions.NULL);

    const listener = recordFunction();
    vh = new PageVisibilityMonitor();

    events.listen(vh, 'visibilitychange', listener);

    const e = new GoogTestingEvent('visibilitychange');
    e.target = window.document;
    testingEvents.fireBrowserEvent(e);
    assertEquals(0, listener.getCallCount());
  },

  testListener() {
    stubs.set(
        PageVisibilityMonitor.prototype, 'getBrowserEventType_',
        functions.constant('visibilitychange'));

    const listener = recordFunction();
    vh = new PageVisibilityMonitor();

    events.listen(vh, 'visibilitychange', listener);

    const e = new GoogTestingEvent('visibilitychange');
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    e.target = window.document;
    testingEvents.fireBrowserEvent(e);

    assertEquals(1, listener.getCallCount());
  },

  testListenerForWebKit() {
    stubs.set(
        PageVisibilityMonitor.prototype, 'getBrowserEventType_',
        functions.constant('webkitvisibilitychange'));

    const listener = recordFunction();
    vh = new PageVisibilityMonitor();

    events.listen(vh, 'visibilitychange', listener);

    const e = new GoogTestingEvent('webkitvisibilitychange');
    /**
     * @suppress {constantProperty} suppression added to enable type checking
     */
    e.target = window.document;
    testingEvents.fireBrowserEvent(e);

    assertEquals(1, listener.getCallCount());
  },
});
