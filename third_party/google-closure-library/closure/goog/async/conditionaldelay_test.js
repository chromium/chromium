/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.async.ConditionalDelayTest');
goog.setTestOnly();

const ConditionalDelay = goog.require('goog.async.ConditionalDelay');
const MockClock = goog.require('goog.testing.MockClock');
const testSuite = goog.require('goog.testing.testSuite');

let invoked = false;
let delay = null;
let clock = null;
let returnValue = true;
let onSuccessCalled = false;
let onFailureCalled = false;

function callback() {
  invoked = true;
  return returnValue;
}

testSuite({
  setUp() {
    clock = new MockClock(true);
    invoked = false;
    returnValue = true;
    onSuccessCalled = false;
    onFailureCalled = false;
    delay = new ConditionalDelay(callback);
    delay.onSuccess = () => {
      onSuccessCalled = true;
    };
    delay.onFailure = () => {
      onFailureCalled = true;
    };
  },

  tearDown() {
    clock.dispose();
    delay.dispose();
  },

  testDelay() {
    delay.start(200, 200);
    assertFalse(invoked);

    clock.tick(100);
    assertFalse(invoked);

    clock.tick(100);
    assertTrue(invoked);
  },

  testStop() {
    delay.start(200, 500);
    assertTrue(delay.isActive());

    clock.tick(100);
    assertFalse(invoked);

    delay.stop();
    clock.tick(100);
    assertFalse(invoked);

    assertFalse(delay.isActive());
  },

  testIsActive() {
    assertFalse(delay.isActive());
    delay.start(200, 200);
    assertTrue(delay.isActive());
    clock.tick(200);
    assertFalse(delay.isActive());
  },

  testRestart() {
    delay.start(200, 50000);
    clock.tick(100);

    delay.stop();
    assertFalse(invoked);

    delay.start(200, 50000);
    clock.tick(199);
    assertFalse(invoked);

    clock.tick(1);
    assertTrue(invoked);

    invoked = false;
    delay.start(200, 200);
    clock.tick(200);
    assertTrue(invoked);

    assertFalse(delay.isActive());
  },

  testDispose() {
    delay.start(200, 200);
    delay.dispose();
    assertTrue(delay.isDisposed());

    clock.tick(500);
    assertFalse(invoked);
  },

  testConditionalDelay_Success() {
    returnValue = false;
    delay.start(100, 300);

    clock.tick(99);
    assertFalse(invoked);
    clock.tick(1);
    assertTrue(invoked);

    assertTrue(delay.isActive());
    assertFalse(delay.isDone());
    assertFalse(onSuccessCalled);
    assertFalse(onFailureCalled);

    returnValue = true;

    invoked = false;
    clock.tick(100);
    assertTrue(invoked);

    assertFalse(delay.isActive());
    assertTrue(delay.isDone());
    assertTrue(onSuccessCalled);
    assertFalse(onFailureCalled);

    invoked = false;
    clock.tick(200);
    assertFalse(invoked);
  },

  testConditionalDelay_Failure() {
    returnValue = false;
    delay.start(100, 300);

    clock.tick(99);
    assertFalse(invoked);
    clock.tick(1);
    assertTrue(invoked);

    assertTrue(delay.isActive());
    assertFalse(delay.isDone());
    assertFalse(onSuccessCalled);
    assertFalse(onFailureCalled);

    invoked = false;
    clock.tick(100);
    assertTrue(invoked);
    assertFalse(onSuccessCalled);
    assertFalse(onFailureCalled);

    invoked = false;
    clock.tick(90);
    assertFalse(invoked);
    clock.tick(10);
    assertTrue(invoked);

    assertFalse(delay.isActive());
    assertFalse(delay.isDone());
    assertFalse(onSuccessCalled);
    assertTrue(onFailureCalled);
  },

  testInfiniteDelay() {
    returnValue = false;
    delay.start(100, -1);

    // Test in a big enough loop.
    for (let i = 0; i < 1000; ++i) {
      clock.tick(80);
      assertTrue(delay.isActive());
      assertFalse(delay.isDone());
      assertFalse(onSuccessCalled);
      assertFalse(onFailureCalled);
    }

    delay.stop();
    assertFalse(delay.isActive());
    assertFalse(delay.isDone());
    assertFalse(onSuccessCalled);
    assertFalse(onFailureCalled);
  },

  testCallbackScope() {
    let callbackCalled = false;
    const scopeObject = {};
    function internalCallback() {
      assertEquals(this, scopeObject);
      callbackCalled = true;
      return true;
    }
    delay = new ConditionalDelay(internalCallback, scopeObject);
    delay.start(200, 200);
    clock.tick(201);
    assertTrue(callbackCalled);
  },
});
