/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.messaging.PortNetworkTest');
goog.setTestOnly();

const GoogPromise = goog.require('goog.Promise');
const PortChannel = goog.require('goog.messaging.PortChannel');
const PortOperator = goog.require('goog.messaging.PortOperator');
const TestCase = goog.require('goog.testing.TestCase');
const Timer = goog.require('goog.Timer');
const browser = goog.require('goog.labs.userAgent.browser');
const dispose = goog.require('goog.dispose');
const testSuite = goog.require('goog.testing.testSuite');

let timer;

function shouldRunTests() {
  // TODO(user): This test fails when run in a suite immediately after
  // portchannel_test. The workers take dozens of seconds to start up for some
  // reason.
  return !browser.isEdge();
}

testSuite({
  setUpPage() {
    // Use a relatively long timeout because workers can take a while to start
    // up.
    TestCase.getActiveTestCase().promiseTimeout = 60 * 1000;
  },

  setUp() {
    timer = new Timer(50);
  },

  tearDown() {
    dispose(timer);
  },

  testRouteMessageThroughWorkers() {
    if (!('MessageChannel' in globalThis)) {
      return;
    }

    const master = new PortOperator('main');
    master.addPort(
        'worker1',
        new PortChannel(new Worker('testdata/portnetwork_worker1.js')));
    master.addPort(
        'worker2',
        new PortChannel(new Worker('testdata/portnetwork_worker2.js')));
    const peerOrigin = window.location.protocol + '//' + window.location.host;
    master.addPort(
        'frame',
        PortChannel.forEmbeddedWindow(
            window.frames['inner'], peerOrigin, timer));

    const promise = new GoogPromise((resolve, reject) => {
      master.dial('worker1').registerService('result', resolve, true);
    });
    master.dial('worker2').send('sendToFrame', ['main']);

    return promise
        .then((msg) => {
          assertArrayEquals(['main', 'worker2', 'frame', 'worker1'], msg);
        })
        .thenAlways(() => {
          master.dispose();
        });
  },
});
