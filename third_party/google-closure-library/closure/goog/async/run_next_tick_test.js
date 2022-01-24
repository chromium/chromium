/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @fileoverview tests run using its goog.async.nextTick codepath
 * activated via MockClock.
 */

goog.module('goog.async.runNextTickTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const dispose = goog.require('goog.dispose');
const recordFunction = goog.require('goog.testing.recordFunction');
const run = goog.require('goog.async.run');
const testSuite = goog.require('goog.testing.testSuite');

let mockClock;

/** @type {?} */
let futureCallback1;

/** @type {?} */
let futureCallback2;

testSuite({
  setUpPage() {
    mockClock = new MockClock();
    mockClock.install();
  },

  setUp() {
    mockClock.reset();
    futureCallback1 = recordFunction();
    futureCallback2 = recordFunction();
  },

  tearDown() {
    futureCallback1 = null;
    futureCallback2 = null;
  },

  tearDownPage() {
    mockClock.uninstall();
    dispose(mockClock);
  },

  testCalledAsync() {
    run(futureCallback1);
    run(futureCallback2);

    assertEquals(0, futureCallback1.getCallCount());
    assertEquals(0, futureCallback2.getCallCount());

    // but the callbacks are scheduled...
    mockClock.tick();

    // and called.
    assertEquals(1, futureCallback1.getCallCount());
    assertEquals(1, futureCallback2.getCallCount());

    // and aren't called a second time.
    assertEquals(1, futureCallback1.getCallCount());
    assertEquals(1, futureCallback2.getCallCount());
  },

  testSequenceCalledInOrder() {
    futureCallback1 = recordFunction(() => {
      // called before futureCallback2
      assertEquals(0, futureCallback2.getCallCount());
    });
    futureCallback2 = recordFunction(() => {
      // called after futureCallback1
      assertEquals(1, futureCallback1.getCallCount());
    });
    run(futureCallback1);
    run(futureCallback2);

    // goog.async.run doesn't call the top callback immediately.
    assertEquals(0, futureCallback1.getCallCount());

    // but the callbacks are scheduled...
    mockClock.tick();

    // and called during the same "tick".
    assertEquals(1, futureCallback1.getCallCount());
    assertEquals(1, futureCallback2.getCallCount());
  },

  testSequenceScheduledTwice() {
    run(futureCallback1);
    run(futureCallback1);

    // goog.async.run doesn't call the top callback immediately.
    assertEquals(0, futureCallback1.getCallCount());

    // but the callbacks are scheduled...
    mockClock.tick();

    // and called twice during the same "tick".
    assertEquals(2, futureCallback1.getCallCount());
  },

  testSequenceCalledSync() {
    futureCallback1 = recordFunction(() => {
      run(futureCallback2);
      // goog.async.run doesn't call the inner callback immediately.
      assertEquals(0, futureCallback2.getCallCount());
    });
    run(futureCallback1);

    // goog.async.run doesn't call the top callback immediately.
    assertEquals(0, futureCallback1.getCallCount());

    // but the callbacks are scheduled...
    mockClock.tick();

    // and called during the same "tick".
    assertEquals(1, futureCallback1.getCallCount());
    assertEquals(1, futureCallback2.getCallCount());
  },

  testScope() {
    const aScope = {};
    run(futureCallback1);
    run(futureCallback2, aScope);

    // the callbacks are scheduled...
    mockClock.tick();

    // and called.
    assertEquals(1, futureCallback1.getCallCount());
    assertEquals(1, futureCallback2.getCallCount());

    // and get the correct scope.
    const last1 = futureCallback1.popLastCall();
    assertEquals(0, last1.getArguments().length);
    assertEquals(undefined, last1.getThis());

    const last2 = futureCallback2.popLastCall();
    assertEquals(0, last2.getArguments().length);
    assertEquals(aScope, last2.getThis());
  },
});
