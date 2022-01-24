/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.TimerTest');
goog.setTestOnly();

const GoogPromise = goog.require('goog.Promise');
const MockClock = goog.require('goog.testing.MockClock');
const Timer = goog.require('goog.Timer');
const events = goog.require('goog.events');
const testSuite = goog.require('goog.testing.testSuite');

const intervalIds = {};
const intervalIdCounter = 0;
let mockClock;
const maxDuration = 60 * 1000;  // 60s

// Run a test for 60s and see how many counts we get
function runTest(string, ticks, number) {
  const t = new Timer(ticks);
  let count = 0;
  events.listen(t, 'tick', (evt) => {
    count++;
  });
  t.start();
  mockClock.tick(maxDuration);
  assertEquals(string, number, count);
  t.stop();
  events.removeAll(t);
}

testSuite({
  setUp() {
    mockClock = new MockClock(true /* install */);
  },

  tearDown() {
    mockClock.dispose();
  },

  test100msTicks() {
    // Desc, interval in ms, expected ticks in 60s
    runTest('10 ticks per second for 60 seconds', 100, 600);
  },

  test500msTicks() {
    runTest('2 ticks per second for 60 seconds', 500, 120);
  },

  test1sTicks() {
    runTest('1 tick per second for 60 seconds', 1000, 60);
  },

  test2sTicks() {
    runTest('1 tick every 2 seconds for 60 seconds', 2000, 30);
  },

  test5sTicks() {
    runTest('1 tick every 5 seconds for 60 seconds', 5000, 12);
  },

  test10sTicks() {
    runTest('1 tick every 10 seconds for 60 seconds', 10000, 6);
  },

  test30sTicks() {
    runTest('1 tick every 30 seconds for 60 seconds', 30000, 2);
  },

  test60sTicks() {
    runTest('1 tick every 60 seconds', 60000, 1);
  },

  testCallOnce() {
    let c = 0;
    const expectedTimeoutId = MockClock.nextId;
    const actualTimeoutId = Timer.callOnce(() => {
      if (c > 0) {
        assertTrue('callOnce should only be called once', false);
      }
      c++;
    });
    assertEquals(
        'callOnce should return the timeout ID', expectedTimeoutId,
        actualTimeoutId);

    const obj = {c: 0};
    Timer.callOnce(function() {
      if (this.c > 0) {
        assertTrue('callOnce should only be called once', false);
      }
      assertEquals(obj, this);
      this.c++;
    }, 1, obj);
    mockClock.tick(maxDuration);
  },

  testCallOnceIgnoresTimeoutsTooLarge() {
    const failCallback = goog.partial(fail, 'Timeout should never be called');
    assertEquals(
        'Timeouts slightly too large should yield a timer ID of -1', -1,
        Timer.callOnce(failCallback, 2147483648));
    assertEquals(
        'Infinite timeouts should yield a timer ID of -1', -1,
        Timer.callOnce(failCallback, Infinity));
  },

  testPromise() {
    let c = 0;
    Timer.promise(1, 'A').then((result) => {
      c++;
      assertEquals('promise should return resolved value', 'A', result);
    });
    mockClock.tick(10);
    assertEquals('promise must be yielded once and only once', 1, c);
  },

  testPromise_cancel() {
    let c = 0;
    Timer.promise(1, 'A')
        .then(
            (result) => {
              fail('promise must not be resolved');
            },
            (reason) => {
              c++;
              assertTrue(
                  'promise must fail due to cancel signal',
                  reason instanceof GoogPromise.CancellationError);
            })
        .cancel();
    mockClock.tick(10);
    assertEquals('promise must be canceled once and only once', 1, c);
  },

  testPromise_timeoutTooLarge() {
    let c = 0;
    Timer.promise(2147483648, 'A')
        .then(
            (result) => {
              fail('promise must not be resolved');
            },
            (reason) => {
              c++;
              assertTrue('promise must be rejected', reason instanceof Error);
            });
    mockClock.tick(10);
    assertEquals('promise must be rejected once and only once', 1, c);
  },

  testStartInTickIsNoOp() {
    const pendingTimeouts = new Set();
    const obj = {
      setTimeout: function(callback) {
        const id = setTimeout(() => {
          pendingTimeouts.delete(id);
          callback();
        }, 1);
        pendingTimeouts.add(id);
        return id;
      },
      clearTimeout: function(id) {
        pendingTimeouts.delete(id);
        clearTimeout(id);
      },
    };
    const t = new Timer(0, obj);
    let ticks = 0;
    events.listen(t, 'tick', () => ticks++);
    t.start();
    assertEquals(1, pendingTimeouts.size);
    assertEquals(0, ticks);
    mockClock.tick();
    assertEquals(1, pendingTimeouts.size);
    assertEquals(1, ticks);
    events.listen(t, 'tick', () => t.start());
    mockClock.tick();
    assertEquals(1, pendingTimeouts.size);
    assertEquals(2, ticks);
    t.dispose();
    assertEquals(0, pendingTimeouts.size);
    assertEquals(2, ticks);
    mockClock.tick();
    assertEquals(0, pendingTimeouts.size);
    assertEquals(2, ticks);
  },
});
