/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.messaging.MultiChannelTest');
goog.setTestOnly();

const IgnoreArgument = goog.require('goog.testing.mockmatchers.IgnoreArgument');
const MockControl = goog.require('goog.testing.MockControl');
const MockMessageChannel = goog.require('goog.testing.messaging.MockMessageChannel');
const MultiChannel = goog.require('goog.messaging.MultiChannel');
const testSuite = goog.require('goog.testing.testSuite');

let mockControl;
let mockChannel;
let multiChannel;
let channel0;
let channel1;
let channel2;

function expectedFn(name, callback) {
  const ignored = new IgnoreArgument();
  const fn = mockControl.createFunctionMock(name);
  fn(ignored).$does(function(args) { callback.apply(this, args); });
  return function() { fn(arguments); };
}

function notExpectedFn() {
  return mockControl.createFunctionMock('notExpectedFn');
}

function assertEqualsFn() {
  const expectedArgs = Array.prototype.slice.call(arguments);
  return expectedFn('assertEqualsFn', function() {
    assertObjectEquals(expectedArgs, Array.prototype.slice.call(arguments));
  });
}

testSuite({
  setUp() {
    mockControl = new MockControl();
    mockChannel = new MockMessageChannel(mockControl);
    multiChannel = new MultiChannel(mockChannel);
    channel0 = multiChannel.createVirtualChannel('foo');
    channel1 = multiChannel.createVirtualChannel('bar');
  },

  tearDown() {
    multiChannel.dispose();
    mockControl.$verifyAll();
    assertTrue(mockChannel.disposed);
  },

  testSend0() {
    mockChannel.send('foo:fooBar', {foo: 'bar'});
    mockControl.$replayAll();
    channel0.send('fooBar', {foo: 'bar'});
  },

  testSend1() {
    mockChannel.send('bar:fooBar', {foo: 'bar'});
    mockControl.$replayAll();
    channel1.send('fooBar', {foo: 'bar'});
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testReceive0() {
    channel0.registerService('fooBar', assertEqualsFn('Baz bang'));
    channel1.registerService('fooBar', notExpectedFn());
    mockControl.$replayAll();
    mockChannel.receive('foo:fooBar', 'Baz bang');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testReceive1() {
    channel1.registerService('fooBar', assertEqualsFn('Baz bang'));
    channel0.registerService('fooBar', notExpectedFn());
    mockControl.$replayAll();
    mockChannel.receive('bar:fooBar', 'Baz bang');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDefaultReceive0() {
    channel0.registerDefaultService(assertEqualsFn('fooBar', 'Baz bang'));
    channel1.registerDefaultService(notExpectedFn());
    mockControl.$replayAll();
    mockChannel.receive('foo:fooBar', 'Baz bang');
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDefaultReceive1() {
    channel1.registerDefaultService(assertEqualsFn('fooBar', 'Baz bang'));
    channel0.registerDefaultService(notExpectedFn());
    mockControl.$replayAll();
    mockChannel.receive('bar:fooBar', 'Baz bang');
  },

  testReceiveAfterDisposed() {
    channel0.registerService('fooBar', notExpectedFn());
    mockControl.$replayAll();
    channel0.dispose();
    mockChannel.receive('foo:fooBar', 'Baz bang');
  },

  testReceiveAfterParentDisposed() {
    channel0.registerService('fooBar', notExpectedFn());
    mockControl.$replayAll();
    multiChannel.dispose();
    mockChannel.receive('foo:fooBar', 'Baz bang');
  },
});
