/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.messaging.PortOperatorTest');
goog.setTestOnly();

const MockControl = goog.require('goog.testing.MockControl');
const MockMessageChannel = goog.require('goog.testing.messaging.MockMessageChannel');
const MockMessagePort = goog.require('goog.testing.messaging.MockMessagePort');
const PortNetwork = goog.require('goog.messaging.PortNetwork');
const PortOperator = goog.require('goog.messaging.PortOperator');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');

let stubs;

let mockControl;
let mockChannel1;
let mockChannel2;
let operator;

function makeMockPort(index, port) {
  return new MockMessagePort({index: index, port: port}, mockControl);
}

testSuite({
  setUpPage() {
    stubs = new PropertyReplacer();
  },

  setUp() {
    mockControl = new MockControl();
    let index = 0;
    stubs.set(globalThis, 'MessageChannel', function() {
      this.port1 = makeMockPort(index, 1);
      this.port2 = makeMockPort(index, 2);
      index += 1;
    });

    mockChannel1 = new MockMessageChannel(mockControl);
    mockChannel2 = new MockMessageChannel(mockControl);
    operator = new PortOperator('operator');
    operator.addPort('1', mockChannel1);
    operator.addPort('2', mockChannel2);
  },

  tearDown() {
    dispose(operator);
    mockControl.$verifyAll();
    stubs.reset();
  },

  testConnectSelfToPortViaRequestConnection() {
    mockChannel1.send(
        PortNetwork.GRANT_CONNECTION_SERVICE,
        {success: true, name: 'operator', port: makeMockPort(0, 1)});
    mockControl.$replayAll();
    mockChannel1.receive(PortNetwork.REQUEST_CONNECTION_SERVICE, 'operator');
    const port = operator.dial('1').port_;
    assertObjectEquals({index: 0, port: 2}, port.id);
    assertEquals(true, port.started);
  },

  testConnectSelfToPortViaGetPort() {
    mockChannel1.send(
        PortNetwork.GRANT_CONNECTION_SERVICE,
        {success: true, name: 'operator', port: makeMockPort(0, 1)});
    mockControl.$replayAll();
    const port = operator.dial('1').port_;
    assertObjectEquals({index: 0, port: 2}, port.id);
    assertEquals(true, port.started);
  },

  testConnectTwoCallers() {
    mockChannel1.send(
        PortNetwork.GRANT_CONNECTION_SERVICE,
        {success: true, name: '2', port: makeMockPort(0, 1)});
    mockChannel2.send(
        PortNetwork.GRANT_CONNECTION_SERVICE,
        {success: true, name: '1', port: makeMockPort(0, 2)});
    mockControl.$replayAll();
    mockChannel1.receive(PortNetwork.REQUEST_CONNECTION_SERVICE, '2');
  },

  testConnectCallerToNonexistentCaller() {
    mockChannel1.send(PortNetwork.GRANT_CONNECTION_SERVICE, {
      success: false,
      message: 'Port "1" requested a connection to port "no", which doesn\'t ' +
          'exist',
    });
    mockControl.$replayAll();
    mockChannel1.receive(PortNetwork.REQUEST_CONNECTION_SERVICE, 'no');
  },
});
