/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.IframeLoadMonitorTest');
goog.setTestOnly('goog.net.IframeLoadMonitorTest');

const IframeLoadMonitor = goog.require('goog.net.IframeLoadMonitor');
const Promise = goog.require('goog.Promise');
const TagName = goog.require('goog.dom.TagName');
const Timer = goog.require('goog.Timer');
const dom = goog.require('goog.dom');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');


const TEST_FRAME_SRC = 'iframeloadmonitor_test_frame.html';
let frameParent;


testSuite({
  setUpPage: function() {
    frameParent = dom.getElement('frame_parent');
  },

  tearDown: function() {
    dom.removeChildren(frameParent);
  },

  testIframeLoadMonitor: function() {
    const frame = dom.createDom(TagName.IFRAME);
    const monitor = new IframeLoadMonitor(frame);
    const monitorPromise = new Promise(function(resolve, reject) {
      events.listen(monitor, IframeLoadMonitor.LOAD_EVENT, resolve);
    });

    assertFalse(monitor.isLoaded());
    frameParent.appendChild(frame);

    return monitorPromise.then(function(e) {
      assertEquals(IframeLoadMonitor.LOAD_EVENT, e.type);
      assertTrue(monitor.isLoaded());
    });
  },

  testIframeLoadMonitor_withContentCheck: function() {
    const frame = dom.createDom(TagName.IFRAME);
    const monitor = new IframeLoadMonitor(frame, true);
    const monitorPromise = new Promise(function(resolve, reject) {
      events.listen(monitor, IframeLoadMonitor.LOAD_EVENT, resolve);
    });

    assertFalse(monitor.isLoaded());
    frameParent.appendChild(frame);

    return Timer.promise(10)
        .then(function() {
          assertFalse(
              'Monitor should not fire before content has loaded.',
              monitor.isLoaded());
          frame.src = TEST_FRAME_SRC;

          return monitorPromise;
        })
        .then(function(e) {
          assertEquals(IframeLoadMonitor.LOAD_EVENT, e.type);
          assertTrue(monitor.isLoaded());
        });
  },
});
