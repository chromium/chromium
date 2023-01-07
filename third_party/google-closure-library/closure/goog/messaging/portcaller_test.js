/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.messaging.PortCallerTest');
goog.setTestOnly();

const GoogEventTarget = goog.require('goog.events.EventTarget');
const MockControl = goog.require('goog.testing.MockControl');
const MockMessageChannel = goog.require('goog.testing.messaging.MockMessageChannel');
const PortCaller = goog.require('goog.messaging.PortCaller');
const PortNetwork = goog.require('goog.messaging.PortNetwork');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');

let mockControl;
let mockChannel;
let caller;

class MockMessagePort extends GoogEventTarget {
  constructor(index, port) {
    super();
    this.index = index;
    this.port = port;
    this.started = false;
  }

  start() {
    this.started = true;
  }
}

testSuite({
  setUp() {
    mockControl = new MockControl();
    mockChannel = new MockMessageChannel(mockControl);
    caller = new PortCaller(mockChannel);
  },

  tearDown() {
    dispose(caller);
    mockControl.$verifyAll();
  },

  testGetPort() {
    mockChannel.send(PortNetwork.REQUEST_CONNECTION_SERVICE, 'foo');
    mockControl.$replayAll();
    caller.dial('foo');
  },
});
