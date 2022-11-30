/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.net.MultiIframeLoadMonitorTest');
goog.setTestOnly('goog.net.MultiIframeLoadMonitorTest');

const MultiIframeLoadMonitor = goog.require('goog.net.MultiIframeLoadMonitor');
const Promise = goog.require('goog.Promise');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TagName = goog.require('goog.dom.TagName');
const Timer = goog.require('goog.Timer');
const dom = goog.require('goog.dom');
const testSuite = goog.require('goog.testing.testSuite');


const stubs = new PropertyReplacer();
const TEST_FRAME_SRCS =
    ['iframeloadmonitor_test_frame.html', 'iframeloadmonitor_test_frame2.html'];
let frameParent;

testSuite({
  setUpPage: function() {
    frameParent = dom.getElement('frame_parent');
  },

  tearDown: function() {
    dom.removeChildren(frameParent);
    stubs.reset();
  },

  testMultiIframeLoadMonitor: function() {
    const frames =
        [dom.createDom(TagName.IFRAME), dom.createDom(TagName.IFRAME)];
    let loaded = false;

    const monitorPromise = new Promise(function(resolve, reject) {
      new MultiIframeLoadMonitor(frames, function() {
        loaded = true;
        resolve();
      });
    });

    assertFalse(loaded);
    frameParent.appendChild(frames[0]);
    frameParent.appendChild(frames[1]);

    return monitorPromise.then(function() { assertTrue(loaded); });
  },

  testMultiIframeLoadMonitor_withContentCheck: function() {
    const frames =
        [dom.createDom(TagName.IFRAME), dom.createDom(TagName.IFRAME)];
    let loaded = false;

    const monitorPromise = new Promise(function(resolve, reject) {
      new MultiIframeLoadMonitor(frames, function() {
        loaded = true;
        resolve();
      }, true);
    });

    frameParent.appendChild(frames[0]);
    frameParent.appendChild(frames[1]);

    return Timer.promise(10)
        .then(function() {
          assertFalse(
              'Monitor should not fire until all iframes have content.',
              loaded);

          frames[0].src = TEST_FRAME_SRCS[0];
          return Timer.promise(10);
        })
        .then(function() {
          assertFalse(
              'Monitor should not fire until all iframes have content.',
              loaded);

          frames[1].src = TEST_FRAME_SRCS[1];
          return monitorPromise;
        })
        .then(function() { assertTrue(loaded); });
  },

  testStopMonitoring: function() {
    let iframeLoadMonitorsCreated = 0;
    let disposeCalls = 0;

    // Replace the IframeLoadMonitor implementation with a fake.
    function FakeIframeLoadMonitor() {
      iframeLoadMonitorsCreated++;
      return {
        attachEvent: function() {},
        dispose: function() {
          disposeCalls++;
        },
        isLoaded: function() {
          return false;
        },
      };
    }
    FakeIframeLoadMonitor.LOAD_EVENT = 'ifload';
    stubs.replace(goog.net, 'IframeLoadMonitor', FakeIframeLoadMonitor);

    const frames =
        [dom.createDom(TagName.IFRAME), dom.createDom(TagName.IFRAME)];
    const monitor = new MultiIframeLoadMonitor(frames, function() {
      fail('should not invoke callback for unloaded frames');
    }, true);

    assertEquals(frames.length, iframeLoadMonitorsCreated);
    assertEquals(0, disposeCalls);

    monitor.stopMonitoring();
    assertEquals(frames.length, disposeCalls);
  },
});
