/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Tests for PixelDensityMonitor. */

goog.module('goog.labs.style.PixelDensityMonitorTest');
goog.setTestOnly();

const DomHelper = goog.require('goog.dom.DomHelper');
const MockControl = goog.require('goog.testing.MockControl');
const PixelDensityMonitor = goog.require('goog.labs.style.PixelDensityMonitor');
const dispose = goog.require('goog.dispose');
const events = goog.require('goog.events');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');
const testingRecordFunction = goog.require('goog.testing.recordFunction');

let fakeWindow;
let recordFunction;
let monitor;
let mockControl;
let mediaQueryLists;

/**
 * Options for setUpMonitor.
 * @record
 */
class MonitorOpts {
  constructor() {
    /**
     * Whether to mock a matchMedia object on fakeWindow.
     * @type {boolean|undefined}
     */
    this.hasMatchMedia;
    /**
     * Whether to delete the deprecated addListener method that isn't
     * implemented in all browsers.  Requires hasMatchMedia to be true.
     * @type {boolean|undefined}
     */
    this.deprecatedAddListener;
    /**
     * Whether to test media query list with `addEventListener` method.
     * @type {boolean|undefined}
     */
    this.mockAddEventListener;
  }
}

/**
 * Helper to create `matchMedia` mock and assign to `fakeWindow`. Used to mock
 * different browser compatibilities.
 *
 * @param {number|undefined} initialRatio
 * @param {!MonitorOpts=} options
 */
function setUpMonitor(initialRatio, {
  hasMatchMedia = false,
  deprecatedAddListener = false,
  mockAddEventListener = false
} = {}) {
  fakeWindow = {devicePixelRatio: initialRatio};

  if (hasMatchMedia) {
    // Every call to matchMedia should return a new media query list with its
    // own set of listeners.
    fakeWindow.matchMedia = (query) => {
      const listeners = [];
      const newList = {
        addListener: function(listener) {
          listeners.push(listener);
        },
        removeListener: function(listener) {
          googArray.remove(listeners, listener);
        },
        callListeners: function() {
          for (let i = 0; i < listeners.length; i++) {
            listeners[i]();
          }
        },
        getListenerCount: function() {
          return listeners.length;
        },
      };
      if (mockAddEventListener) {
        newList.addEventListener = function(target, listener) {
          if (target === 'change') {
            listeners.push(listener);
          }
        };
        newList.removeEventListener = function(target, listener) {
          if (target === 'change') {
            googArray.remove(listeners, listener);
          }
        };
      }
      // Regression testing Cobalt browser compatibility (https://cobalt.dev/).
      // Cobalt browser has `matchMedia` but doesn't implement the deprecated
      // addListener method.
      if (deprecatedAddListener) {
        newList.addListener = undefined;
      }
      mediaQueryLists.push(newList);
      return newList;
    };
  }

  const domHelper = mockControl.createStrictMock(DomHelper);
  domHelper.getWindow().$returns(fakeWindow);
  mockControl.$replayAll();

  monitor = new PixelDensityMonitor(domHelper);
  events.listen(monitor, PixelDensityMonitor.EventType.CHANGE, recordFunction);
}

/**
 * @param {number} newRatio
 */
function setNewRatio(newRatio) {
  fakeWindow.devicePixelRatio = newRatio;
  for (let i = 0; i < mediaQueryLists.length; i++) {
    mediaQueryLists[i].callListeners();
  }
}

testSuite({
  setUp() {
    recordFunction = testingRecordFunction();
    mediaQueryLists = [];
    mockControl = new MockControl();
  },

  tearDown() {
    mockControl.$verifyAll();
    dispose(monitor);
    dispose(recordFunction);
  },

  testNormalDensity() {
    setUpMonitor(1);
    assertEquals(PixelDensityMonitor.Density.NORMAL, monitor.getDensity());
  },

  testHighDensity() {
    setUpMonitor(1.5);
    assertEquals(PixelDensityMonitor.Density.HIGH, monitor.getDensity());
  },

  testNormalDensityIfUndefined() {
    setUpMonitor(undefined);
    assertEquals(PixelDensityMonitor.Density.NORMAL, monitor.getDensity());
  },

  testChangeEvent() {
    setUpMonitor(1, {hasMatchMedia: true});
    assertEquals(PixelDensityMonitor.Density.NORMAL, monitor.getDensity());
    monitor.start();

    setNewRatio(2);
    let call = recordFunction.popLastCall();
    assertEquals(
        PixelDensityMonitor.Density.HIGH,
        call.getArgument(0).target.getDensity());
    assertEquals(PixelDensityMonitor.Density.HIGH, monitor.getDensity());

    setNewRatio(1);
    call = recordFunction.popLastCall();
    assertEquals(
        PixelDensityMonitor.Density.NORMAL,
        call.getArgument(0).target.getDensity());
    assertEquals(PixelDensityMonitor.Density.NORMAL, monitor.getDensity());
  },

  testEventListenerChangeEvent() {
    setUpMonitor(1, {
      hasMatchMedia: true,
      deprecatedAddListener: true,
      mockAddEventListener: true
    });
    assertEquals(PixelDensityMonitor.Density.NORMAL, monitor.getDensity());
    monitor.start();

    setNewRatio(2);
    let call = recordFunction.popLastCall();
    assertEquals(
        PixelDensityMonitor.Density.HIGH,
        call.getArgument(0).target.getDensity());
    assertEquals(PixelDensityMonitor.Density.HIGH, monitor.getDensity());

    setNewRatio(1);
    call = recordFunction.popLastCall();
    assertEquals(
        PixelDensityMonitor.Density.NORMAL,
        call.getArgument(0).target.getDensity());
    assertEquals(PixelDensityMonitor.Density.NORMAL, monitor.getDensity());
  },

  testListenerIsDisposed() {
    setUpMonitor(1, {hasMatchMedia: true});
    monitor.start();

    assertEquals(1, mediaQueryLists.length);
    assertEquals(1, mediaQueryLists[0].getListenerCount());

    dispose(monitor);

    assertEquals(1, mediaQueryLists.length);
    assertEquals(0, mediaQueryLists[0].getListenerCount());
  },

  testEventListenerIsDisposed() {
    setUpMonitor(1, {
      hasMatchMedia: true,
      deprecatedAddListener: true,
      mockAddEventListener: true
    });
    monitor.start();

    assertEquals(1, mediaQueryLists.length);
    assertEquals(1, mediaQueryLists[0].getListenerCount());

    dispose(monitor);

    assertEquals(1, mediaQueryLists.length);
    assertEquals(0, mediaQueryLists[0].getListenerCount());
  },

  testAddListenerMethodNotImplemented() {
    setUpMonitor(1, {hasMatchMedia: true, deprecatedAddListener: true});
    assertNotThrows(() => monitor.start());

    assertEquals(PixelDensityMonitor.Density.NORMAL, monitor.getDensity());
  }
});
