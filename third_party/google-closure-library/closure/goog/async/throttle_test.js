/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
goog.module('goog.async.ThrottleTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const Throttle = goog.require('goog.async.Throttle');
const testSuite = goog.require('goog.testing.testSuite');
const {assertDoesNotRetainReference, assertRetainsReference} = goog.require('goog.testing.objects');

testSuite({
  testThrottle() {
    const clock = new MockClock(true);

    let callBackCount = 0;
    const callBackFunction = () => {
      callBackCount++;
    };

    const throttle = new Throttle(callBackFunction, 100);
    assertEquals(0, callBackCount);
    throttle.fire();
    assertEquals(1, callBackCount);
    throttle.fire();
    assertEquals(1, callBackCount);
    throttle.fire();
    throttle.fire();
    assertEquals(1, callBackCount);
    clock.tick(101);
    assertEquals(2, callBackCount);
    clock.tick(101);
    assertEquals(2, callBackCount);

    throttle.fire();
    assertEquals(3, callBackCount);
    throttle.fire();
    assertEquals(3, callBackCount);
    throttle.stop();
    clock.tick(101);
    assertEquals(3, callBackCount);
    throttle.fire();
    assertEquals(4, callBackCount);
    clock.tick(101);
    assertEquals(4, callBackCount);

    throttle.fire();
    throttle.fire();
    assertEquals(5, callBackCount);
    throttle.pause();
    throttle.resume();
    assertEquals(5, callBackCount);
    throttle.pause();
    clock.tick(101);
    assertEquals(5, callBackCount);
    throttle.resume();
    assertEquals(6, callBackCount);
    clock.tick(101);
    assertEquals(6, callBackCount);
    throttle.pause();
    throttle.fire();
    assertEquals(6, callBackCount);
    clock.tick(101);
    assertEquals(6, callBackCount);
    throttle.resume();
    assertEquals(7, callBackCount);

    throttle.pause();
    throttle.pause();
    clock.tick(101);
    throttle.fire();
    throttle.resume();
    assertEquals(7, callBackCount);
    throttle.resume();
    assertEquals(8, callBackCount);

    throttle.pause();
    throttle.pause();
    throttle.fire();
    throttle.resume();
    clock.tick(101);
    assertEquals(8, callBackCount);
    throttle.resume();
    assertEquals(9, callBackCount);

    clock.uninstall();
  },

  testThrottleScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'y': 0};
    new Throttle(function() {
      ++this['y'];
    }, interval, x).fire();
    assertEquals(1, x['y']);

    mockClock.uninstall();
  },

  testThrottleArgumentBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    let calls = 0;
    const throttle = new Throttle((a, b, c) => {
      ++calls;
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval);

    throttle.fire(3, 'string', false);
    assertEquals(1, calls);

    // fire should always pass the last arguments passed to it into the
    // decorated function, even if called multiple times.
    throttle.fire();
    mockClock.tick(interval / 2);
    throttle.fire(8, null, true);
    throttle.fire(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, calls);

    mockClock.uninstall();
  },

  testThrottleArgumentAndScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'calls': 0};
    const throttle = new Throttle(function(a, b, c) {
      ++this['calls'];
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval, x);

    throttle.fire(3, 'string', false);
    assertEquals(1, x['calls']);

    // fire should always pass the last arguments passed to it into the
    // decorated function, even if called multiple times.
    throttle.fire();
    mockClock.tick(interval / 2);
    throttle.fire(8, null, true);
    throttle.fire(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, x['calls']);

    mockClock.uninstall();
  },

  // Ensure that after the listener is invoked, the arguments are released.
  testThrottleArgumentsAreReleased() {
    const x = {calls: 0};
    const arg = {someProperty: 'foo'};
    const throttle = new Throttle((obj) => {
      assertEquals('foo', obj.someProperty);
      x.calls++;
    }, 1);
    // set up a pending call.
    throttle.pause();
    throttle.fire(arg);
    assertEquals(0, x.calls);
    // sanity check that our search algorithm can find the value
    assertRetainsReference(throttle, arg);

    // invoke the call
    throttle.resume();
    assertEquals(1, x.calls);
    // now make sure that throttle doesn't retain a reference to 'arg'
    assertDoesNotRetainReference(throttle, arg);
  },

});
