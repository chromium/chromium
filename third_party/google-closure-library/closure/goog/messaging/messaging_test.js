/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.messaging.MockMessageChannelTest');
goog.setTestOnly();

const MockControl = goog.require('goog.testing.MockControl');
const MockMessageChannel = goog.require('goog.testing.messaging.MockMessageChannel');
const messaging = goog.require('goog.messaging');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testPipe() {
    const mockControl = new MockControl();
    const ch1 = new MockMessageChannel(mockControl);
    const ch2 = new MockMessageChannel(mockControl);
    ch1.send('ping', 'HELLO');
    ch2.send('pong', {key: 'value'});
    messaging.pipe(ch1, ch2);

    mockControl.$replayAll();
    ch2.receive('ping', 'HELLO');
    ch1.receive('pong', {key: 'value'});
    mockControl.$verifyAll();
  },
});
