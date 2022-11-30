/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.PromiseTest');
goog.setTestOnly();

const GoogPromise = goog.require('goog.Promise');
const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TestCase = goog.require('goog.testing.TestCase');
const Thenable = goog.require('goog.Thenable');
const Timer = goog.require('goog.Timer');
const functions = goog.require('goog.functions');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

// TODO(brenneman):
// - Add tests for interoperability with native Promises where available.
// - Add tests for long stack traces.

/** @type {boolean} */
const SUPPORTS_STRICT_MODE = (function() {
                               return this;
                             })() === undefined;

/**
 * @type {boolean}
 * @suppress {strictMissingProperties}
 * */
const MICROTASKS_EXIST = Promise.toString().indexOf('[native code]') >= 0;

/** @type {!MockClock} */
const mockClock = new MockClock();

/** @type {!PropertyReplacer} */
const stubs = new PropertyReplacer();

let unhandledRejections;

// Simple shared objects used as test values.
const dummy = {
  toString: functions.constant('[object dummy]')
};
const sentinel = {
  toString: functions.constant('[object sentinel]')
};

/**
 * Dummy onfulfilled or onrejected function that should not be called.
 * @param {*} result The result passed into the callback.
 */
function shouldNotCall(result) {
  fail('This should not have been called (result: ' + String(result) + ')');
}

/** @typedef {function(new:IThenable<?>, function(function(?), function(?)))} */
const IThenableCtor = undefined;

const /** !IThenableCtor */ NativeOrGoogPromise = window.Promise || GoogPromise;

/** @implements {IThenable<?>} */
class CountingThenable {
  /** @param {function(function(?), function(?))} immediate */
  constructor(immediate) {
    /** @type {number} */
    this.thenCallCount = 0;
    /** @const @private {!GoogPromise<?>} */
    this.internalPromise_ = new GoogPromise(immediate);
  }

  /**
   * @param {?} value
   * @return {!CountingThenable}
   */
  static resolve(value) {
    return new CountingThenable((resolve) => resolve(value));
  }

  /**
   * @param {?(function(?): ?)=} onResolve
   * @param {?(function(?): ?)=} onReject
   * @param {*=} shouldCount
   * @return {?}
   */
  then(onResolve, onReject, shouldCount) {
    if (shouldCount !== CountingThenable) {
      this.thenCallCount++;
    }
    return CountingThenable.resolve(
        this.internalPromise_.then(onResolve, onReject));
  }
}

/** @implements {IThenable<?>} */
class ThrowingThenable {
  /** @param {?} value */
  constructor(value) {
    /** @const {?} */
    this.value = value;
  }

  /**
   * @param {?(function(?): ?)=} onResolve
   * @param {?(function(?): ?)=} onReject
   * @param {*=} ctx
   * @return {?}
   */
  then(onResolve, onReject, ctx) {
    throw this.value;
  }
}

/**
 * Return an `IThenable` for `(typeof ctor)` that resolves on the next tick.
 *
 * @template T
 * @param {function(new:T, ...?)} ctor
 * @param {?} value
 * @return {T}
 */
function fulfillSoon(ctor, value) {
  return new ctor((resolve, reject) => {
    window.setTimeout(() => resolve(value), 0);
  });
}

/**
 * Return an `IThenable` for `(typeof ctor)` that rejects on the next tick.
 *
 * @template T
 * @param {function(new:T, ...?)} ctor
 * @param {?} value
 * @return {T}
 */
function rejectSoon(ctor, value) {
  return new ctor((resolve, reject) => {
    window.setTimeout(() => reject(value), 0);
  });
}

/**
 * Return a new `IThenable` chained after `thenable` with a delay of one tick.
 *
 * @template T
 * @param {T} thenable
 * @param {function(...?): ?} callback
 * @return {T}
 */
function after(thenable, callback) {
  const always = () => {
    try {
      return fulfillSoon(thenable.constructor, callback());
    } catch (e) {
      return rejectSoon(thenable.constructor, e);
    }
  };

  return thenable.then(always, always, CountingThenable);
}

/**
 * Runs the test, passing it a function to call when its promise chain is
 * complete, and all test assertions are complete.
 * @param {function(function():undefined):undefined} testBody
 * @return {!Promise}
 */
function validatePromiseChain(testBody) {
  return new Promise((promiseChainComplete) => {
    testBody(promiseChainComplete);
  });
}

testSuite({
  setUpPage() {
    TestCase.getActiveTestCase().promiseTimeout = 10000;  // 10s
  },

  setUp() {
    unhandledRejections = recordFunction();
    GoogPromise.setUnhandledRejectionHandler(unhandledRejections);
  },

  tearDown() {
    // The system should leave no pending unhandled rejections. Advance the mock
    // clock (if installed) to catch any rethrows waiting in the queue.
    mockClock.tick(Infinity);
    mockClock.uninstall();
    mockClock.reset();

    stubs.reset();
  },

  testThenIsFulfilled() {
    let timesCalled = 0;

    const p = new GoogPromise((resolve, reject) => {
      resolve(sentinel);
    });
    p.then((value) => {
      timesCalled++;
      assertEquals(sentinel, value);
    });

    assertEquals(
        'then() must return before callbacks are invoked.', 0, timesCalled);

    return p.then(() => {
      assertEquals('onFulfilled must be called exactly once.', 1, timesCalled);
    });
  },

  testThenVoidIsFulfilled() {
    let timesCalled = 0;

    const p = GoogPromise.resolve(sentinel);
    p.thenVoid((value) => {
      timesCalled++;
      assertEquals(sentinel, value);
    });

    assertEquals(
        'thenVoid() must return before callbacks are invoked.', 0, timesCalled);

    return p.then(() => {
      assertEquals('onFulfilled must be called exactly once.', 1, timesCalled);
    });
  },

  testThenIsRejected() {
    let timesCalled = 0;

    const p = GoogPromise.reject(sentinel);
    p.then(shouldNotCall, (value) => {
      timesCalled++;
      assertEquals(sentinel, value);
    });

    assertEquals(
        'then() must return before callbacks are invoked.', 0, timesCalled);

    return p.then(shouldNotCall, () => {
      assertEquals('onRejected must be called exactly once.', 1, timesCalled);
    });
  },

  testThenVoidIsRejected() {
    let timesCalled = 0;

    const p = GoogPromise.reject(sentinel);
    p.thenVoid(shouldNotCall, (value) => {
      timesCalled++;
      assertEquals(sentinel, value);
      assertEquals('onRejected must be called exactly once.', 1, timesCalled);
    });

    assertEquals(
        'thenVoid() must return before callbacks are invoked.', 0, timesCalled);

    return p.then(shouldNotCall, () => {
      assertEquals('onRejected must be called exactly once.', 1, timesCalled);
    });
  },

  testThenAsserts() {
    const p = GoogPromise.resolve();

    let m = assertThrows(() => {
      p.then(/** @type {?} */ ({}));
    });
    assertContains('opt_onFulfilled should be a function.', m.message);

    m = assertThrows(() => {
      p.then(() => {}, /** @type {?} */ ({}));
    });
    assertContains('opt_onRejected should be a function.', m.message);
  },

  testThenVoidAsserts() {
    const p = GoogPromise.resolve();

    let m = assertThrows(() => {
      p.thenVoid(/** @type {?} */ ({}));
    });
    assertContains('opt_onFulfilled should be a function.', m.message);

    m = assertThrows(() => {
      p.thenVoid(() => {}, /** @type {?} */ ({}));
    });
    assertContains('opt_onRejected should be a function.', m.message);
  },

  testOptionalOnFulfilled() {
    return GoogPromise.resolve(sentinel)
        .then(null, null)
        .then(null, shouldNotCall)
        .then((value) => {
          assertEquals(sentinel, value);
        });
  },

  testOptionalOnRejected() {
    return GoogPromise.reject(sentinel)
        .then(null, null)
        .then(shouldNotCall)
        .then(null, (reason) => {
          assertEquals(sentinel, reason);
        });
  },

  testMultipleResolves() {
    let timesCalled = 0;
    let resolvePromise;

    const p = new GoogPromise((resolve, reject) => {
      resolvePromise = resolve;
      resolve('foo');
      resolve('bar');
    });

    p.then((value) => {
      timesCalled++;
      assertEquals('onFulfilled must be called exactly once.', 1, timesCalled);
    });

    // Add one more test for fulfilling after a delay.
    return Timer.promise(10).then(() => {
      resolvePromise('baz');
      assertEquals(1, timesCalled);
    });
  },

  testMultipleRejects() {
    let timesCalled = 0;
    let rejectPromise;

    const p = new GoogPromise((resolve, reject) => {
      rejectPromise = reject;
      reject('foo');
      reject('bar');
    });

    p.then(shouldNotCall, (value) => {
      timesCalled++;
      assertEquals('onRejected must be called exactly once.', 1, timesCalled);
    });

    // Add one more test for rejecting after a delay.
    return Timer.promise(10).then(() => {
      rejectPromise('baz');
      assertEquals(1, timesCalled);
    });
  },

  testAsynchronousThenCalls() {
    const timesCalled = [0, 0, 0, 0];
    const p = new GoogPromise((resolve, reject) => {
      window.setTimeout(() => {
        resolve();
      }, 30);
    });

    p.then(() => {
      timesCalled[0]++;
      assertArrayEquals([1, 0, 0, 0], timesCalled);
    });

    window.setTimeout(() => {
      p.then(() => {
        timesCalled[1]++;
        assertArrayEquals([1, 1, 0, 0], timesCalled);
      });
    }, 10);

    window.setTimeout(() => {
      p.then(() => {
        timesCalled[2]++;
        assertArrayEquals([1, 1, 1, 0], timesCalled);
      });
    }, 20);

    return Timer.promise(40).then(() => p.then(() => {
      timesCalled[3]++;
      assertArrayEquals([1, 1, 1, 1], timesCalled);
    }));
  },

  testResolveWithPromise() {
    let resolveBlocker;
    let hasFulfilled = false;
    const blocker = new GoogPromise((resolve, reject) => {
      resolveBlocker = resolve;
    });

    const p = GoogPromise.resolve(blocker);
    p.then((value) => {
      hasFulfilled = true;
      assertEquals(sentinel, value);
    }, shouldNotCall);

    assertFalse(hasFulfilled);
    resolveBlocker(sentinel);

    return p.then(() => {
      assertTrue(hasFulfilled);
    });
  },

  testResolveWithRejectedPromise() {
    let rejectBlocker;
    let hasRejected = false;
    const blocker = new GoogPromise((resolve, reject) => {
      rejectBlocker = reject;
    });

    const p = GoogPromise.resolve(blocker);
    const child = p.then(shouldNotCall, (reason) => {
      hasRejected = true;
      assertEquals(sentinel, reason);
    });

    assertFalse(hasRejected);
    rejectBlocker(sentinel);

    return child.thenCatch(() => {
      assertTrue(hasRejected);
    });
  },

  testRejectWithPromise() {
    let resolveBlocker;
    let hasFulfilled = false;
    let hasRejected = false;
    const blocker = new GoogPromise((resolve, reject) => {
      resolveBlocker = resolve;
    });

    const p = GoogPromise.reject(blocker);
    const child = p.then((value) => {
      hasFulfilled = true;
      assertEquals(sentinel, value);
    }, shouldNotCall);

    assertFalse(hasFulfilled);
    resolveBlocker(sentinel);

    return child.thenCatch(() => {
      assertTrue(hasRejected);
    });
  },

  testRejectWithRejectedPromise() {
    let rejectBlocker;
    let hasRejected = false;
    const blocker = new GoogPromise((resolve, reject) => {
      rejectBlocker = reject;
    });

    const p = GoogPromise.reject(blocker);
    const child = p.then(shouldNotCall, (reason) => {
      hasRejected = true;
      assertEquals(sentinel, reason);
    });

    assertFalse(hasRejected);
    rejectBlocker(sentinel);

    return child.thenCatch(() => {
      assertTrue(hasRejected);
    });
  },

  testResolveAndReject() {
    let onFulfilledCalled = false;
    let onRejectedCalled = false;
    const p = new GoogPromise((resolve, reject) => {
      resolve();
      reject();
    });

    p.then(
        () => {
          onFulfilledCalled = true;
        },
        () => {
          onRejectedCalled = true;
        });

    return p.then(() => {
      assertTrue(onFulfilledCalled);
      assertFalse(onRejectedCalled);
    });
  },

  testResolveWithSelfRejects() {
    let r;
    const p = new GoogPromise((resolve) => {
      r = resolve;
    });
    r(p);
    return p.then(shouldNotCall, (e) => {
      assertEquals(
          /** @type {!Error} */ (e).message,
          'Promise cannot resolve to itself');
    });
  },

  testResolveWithObjectStringResolves() {
    return GoogPromise.resolve('[object Object]').then((v) => {
      assertEquals(v, '[object Object]');
    });
  },

  testRejectAndResolve() {
    return new GoogPromise((resolve, reject) => {
             reject();
             resolve();
           })
        .then(shouldNotCall, () => true);
  },

  testThenReturnsBeforeCallbackWithFulfill() {
    let thenHasReturned = false;
    const p = GoogPromise.resolve();

    const child = p.then(() => {
      assertTrue(
          'Callback must be called only after then() has returned.',
          thenHasReturned);
    });
    thenHasReturned = true;

    return child;
  },

  testThenReturnsBeforeCallbackWithReject() {
    let thenHasReturned = false;
    const p = GoogPromise.reject();

    const child = p.then(shouldNotCall, () => {
      assertTrue(
          'Callback must be called only after then() has returned.',
          thenHasReturned);
    });
    thenHasReturned = true;

    return child;
  },

  testResolutionOrder() {
    const callbacks = [];
    return GoogPromise.resolve()
        .then(
            () => {
              callbacks.push(1);
            },
            shouldNotCall)
        .then(
            () => {
              callbacks.push(2);
            },
            shouldNotCall)
        .then(
            () => {
              callbacks.push(3);
            },
            shouldNotCall)
        .then(() => {
          assertArrayEquals([1, 2, 3], callbacks);
        });
  },

  testResolutionOrderWithThrow() {
    const callbacks = [];
    const p = GoogPromise.resolve();

    p.then(() => {
      callbacks.push(1);
    }, shouldNotCall);
    const child = p.then(() => {
      callbacks.push(2);
      throw new Error();
    }, shouldNotCall);

    child.then(shouldNotCall, () => {
      // The parent callbacks should be evaluated before the child.
      callbacks.push(4);
    });

    p.then(() => {
      callbacks.push(3);
    }, shouldNotCall);

    return child.then(shouldNotCall, () => {
      callbacks.push(5);
      assertArrayEquals([1, 2, 3, 4, 5], callbacks);
    });
  },

  testResolutionOrderWithNestedThen() {
    const resolver = GoogPromise.withResolver();

    const callbacks = [];
    const p = GoogPromise.resolve();

    p.then(() => {
      callbacks.push(1);
      p.then(() => {
        callbacks.push(3);
        resolver.resolve();
      });
    });
    p.then(() => {
      callbacks.push(2);
    });

    return resolver.promise.then(() => {
      assertArrayEquals([1, 2, 3], callbacks);
    });
  },

  testRejectionOrder() {
    const callbacks = [];
    const p = GoogPromise.reject();

    p.then(shouldNotCall, () => {
      callbacks.push(1);
    });
    p.then(shouldNotCall, () => {
      callbacks.push(2);
    });
    p.then(shouldNotCall, () => {
      callbacks.push(3);
    });

    return p.then(shouldNotCall, () => {
      assertArrayEquals([1, 2, 3], callbacks);
    });
  },

  testRejectionOrderWithThrow() {
    const callbacks = [];
    const p = GoogPromise.reject();

    p.then(shouldNotCall, () => {
      callbacks.push(1);
    });
    p.then(shouldNotCall, () => {
      callbacks.push(2);
      throw new Error();
    });
    p.then(shouldNotCall, () => {
      callbacks.push(3);
    });

    return p.then(shouldNotCall, () => {
      assertArrayEquals([1, 2, 3], callbacks);
    });
  },

  testRejectionOrderWithNestedThen() {
    const resolver = GoogPromise.withResolver();

    const callbacks = [];
    const p = GoogPromise.reject();

    p.then(shouldNotCall, () => {
      callbacks.push(1);
      p.then(shouldNotCall, () => {
        callbacks.push(3);
        resolver.resolve();
      });
    });
    p.then(shouldNotCall, () => {
      callbacks.push(2);
    });

    return resolver.promise.then(() => {
      assertArrayEquals([1, 2, 3], callbacks);
    });
  },

  testBranching() {
    const p = GoogPromise.resolve(2);

    const branch1 =
        p.then((value) => {
           assertEquals('then functions should see the same value', 2, value);
           return value / 2;
         }).then((value) => {
          assertEquals('branch should receive the returned value', 1, value);
        });

    const branch2 =
        p.then((value) => {
           assertEquals('then functions should see the same value', 2, value);
           throw value + 1;
         }).then(shouldNotCall, (reason) => {
          assertEquals('branch should receive the thrown value', 3, reason);
        });

    const branch3 =
        p.then((value) => {
           assertEquals('then functions should see the same value', 2, value);
           return value * 2;
         }).then((value) => {
          assertEquals('branch should receive the returned value', 4, value);
        });

    return GoogPromise.all([branch1, branch2, branch3]);
  },

  testThenReturnsPromise() {
    const parent = GoogPromise.resolve();
    const child = parent.then();

    assertTrue(child instanceof GoogPromise);
    assertNotEquals(
        'The returned Promise must be different from the input.', parent,
        child);
  },

  testThenVoidReturnsUndefined() {
    const parent = GoogPromise.resolve();
    const child = parent.thenVoid();

    assertUndefined(child);
  },

  testBlockingPromise() {
    const p = GoogPromise.resolve();
    let wasFulfilled = false;
    let wasRejected = false;

    const p2 = p.then(() => new GoogPromise((resolve, reject) => {}));

    p2.then(
        () => {
          wasFulfilled = true;
        },
        () => {
          wasRejected = true;
        });

    return Timer.promise(10).then(() => {
      assertFalse('p2 should be blocked on the returned Promise', wasFulfilled);
      assertFalse('p2 should be blocked on the returned Promise', wasRejected);
    });
  },

  testBlockingPromiseFulfilled() {
    const blockingPromise = new GoogPromise((resolve, reject) => {
      window.setTimeout(() => {
        resolve(sentinel);
      }, 0);
    });

    const p = GoogPromise.resolve(dummy);
    const p2 = p.then((value) => blockingPromise);

    return p2.then((value) => {
      assertEquals(sentinel, value);
    });
  },

  testBlockingPromiseRejected() {
    const blockingPromise = new GoogPromise((resolve, reject) => {
      window.setTimeout(() => {
        reject(sentinel);
      }, 0);
    });

    const p = GoogPromise.resolve(blockingPromise);

    return p.then(shouldNotCall, (reason) => {
      assertEquals(sentinel, reason);
    });
  },

  testBlockingThenableFulfilled() {
    const thenable = {
      then: function(onFulfill, onReject) {
        onFulfill(sentinel);
      }
    };

    return GoogPromise.resolve(thenable).then((reason) => {
      assertEquals(sentinel, reason);
    });
  },

  testBlockingThenableRejected() {
    const thenable = {
      then: function(onFulfill, onReject) {
        onReject(sentinel);
      }
    };

    return GoogPromise.resolve(thenable).then(shouldNotCall, (reason) => {
      assertEquals(sentinel, reason);
    });
  },

  testBlockingThenableThrows() {
    const thenable = {
      then: function(onFulfill, onReject) {
        throw sentinel;
      }
    };

    return GoogPromise.resolve(thenable).then(shouldNotCall, (reason) => {
      assertEquals(sentinel, reason);
    });
  },

  testBlockingThenableMisbehaves() {
    const thenable = {
      then: function(onFulfill, onReject) {
        onFulfill(sentinel);
        onFulfill(dummy);
        onReject(dummy);
        throw dummy;
      },
    };

    return GoogPromise.resolve(thenable).then((value) => {
      assertEquals(
          'Only the first resolution of the Thenable should have a result.',
          sentinel, value);
    });
  },

  testNestingThenables() {
    const thenableA = {
      then: function(onFulfill, onReject) {
        onFulfill(sentinel);
      },
    };
    const thenableB = {
      then: function(onFulfill, onReject) {
        onFulfill(thenableA);
      },
    };
    const thenableC = {
      then: function(onFulfill, onReject) {
        onFulfill(thenableB);
      },
    };

    return GoogPromise.resolve(thenableC).then((value) => {
      assertEquals(
          'Should resolve to the fulfillment value of thenableA', sentinel,
          value);
    });
  },

  testNestingThenablesRejected() {
    const thenableA = {
      then: function(onFulfill, onReject) {
        onReject(sentinel);
      }
    };
    const thenableB = {
      then: function(onFulfill, onReject) {
        onReject(thenableA);
      },
    };
    const thenableC = {
      then: function(onFulfill, onReject) {
        onReject(thenableB);
      },
    };

    return GoogPromise.reject(thenableC).then(shouldNotCall, (reason) => {
      assertEquals(
          'Should resolve to rejection reason of thenableA', sentinel, reason);
    });
  },

  testThenCatch() {
    return validatePromiseChain((promiseChainComplete) => {
      let catchCalled = false;
      GoogPromise.reject()
          .thenCatch((reason) => {
            catchCalled = true;
            return sentinel;
          })
          .then((value) => {
            assertTrue(catchCalled);
            assertEquals(sentinel, value);
            promiseChainComplete();
          });
    });
  },

  testThenCatchWithSuccess() {
    return validatePromiseChain((promiseChainComplete) => {
      let catchCalled = false;
      GoogPromise.resolve(sentinel)
          .thenCatch((reason) => {
            catchCalled = true;
            return dummy;
          })
          .then((value) => {
            assertFalse(catchCalled);
            assertEquals(sentinel, value);
            promiseChainComplete();
          });
    });
  },

  testThenCatchThrows() {
    return validatePromiseChain((promiseChainComplete) => {
      GoogPromise.reject(dummy)
          .thenCatch((reason) => {
            assertEquals(dummy, reason);
            throw sentinel;
          })
          .then(
              () => {
                fail('Should have rejected.');
              },
              (value) => {
                assertEquals(sentinel, value);
                promiseChainComplete();
              });
    });
  },

  testRaceWithEmptyList() {
    return GoogPromise.race([]).then((value) => {
      assertUndefined(value);
    });
  },

  testRaceWithFulfill() {
    const c = fulfillSoon(NativeOrGoogPromise, 'c');
    const d = after(c, () => 'd');
    const b = after(d, () => 'b');
    const a = after(b, () => 'a');

    return GoogPromise.race([a, b, c, d])
        .then((value) => {
          assertEquals('c', value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testRaceWithThenables() {
    const c = fulfillSoon(CountingThenable, 'c');
    const d = after(c, () => 'd');
    const b = after(d, () => 'b');
    const a = after(b, () => 'a');

    return GoogPromise.race([a, b, c, d])
        .then((value) => {
          assertEquals('c', value);
          // Ensure that the `then` property was only accessed once by
          // `goog.Promise.race`.
          assertEquals(1, c.thenCallCount);
          // Return the slowest input thenable to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest thenable should resolve eventually.', 'a', value);
        });
  },

  testRaceWithBuiltIns() {
    const c = fulfillSoon(NativeOrGoogPromise, 'c');
    const d = after(c, () => 'd');
    const b = after(d, () => 'b');
    const a = after(d, () => 'a');

    return GoogPromise.race([a, b, c, d])
        .then((value) => {
          assertEquals('c', value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testRaceWithNonThenable() {
    const c = fulfillSoon(GoogPromise, 'c');
    const d = after(c, () => 'd');
    const b = 'b';
    const a = after(d, () => 'a');

    return GoogPromise.race([a, b, c, d])
        .then((value) => {
          assertEquals('b', value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testRaceWithFalseyNonThenable() {
    const c = fulfillSoon(GoogPromise, 'c');
    const d = after(c, () => 'd');
    const b = 0;
    const a = after(d, () => 'a');

    return GoogPromise.race([a, b, c, d])
        .then((value) => {
          assertEquals(0, value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testRaceWithFulfilledBeforeNonThenable() {
    const c = 'c';
    const d = fulfillSoon(GoogPromise, 'd');
    const b = GoogPromise.resolve('b');
    const a = after(d, () => 'a');

    return GoogPromise.race([a, b, c, d])
        .then((value) => {
          assertEquals('b', value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testRaceWithReject() {
    const c = rejectSoon(GoogPromise, 'rejected-c');
    const d = after(c, () => {
      throw 'rejected-d';
    });
    const b = after(d, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => {
      throw 'rejected-a';
    });

    return GoogPromise.race([a, b, c, d])
        .then(
            shouldNotCall,
            (value) => {
              assertEquals('rejected-c', value);
              return a;
            })
        .then(shouldNotCall, (reason) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'rejected-a',
              reason);
        });
  },

  testRaceWithRejectThenable() {
    const c = rejectSoon(CountingThenable, 'rejected-c');
    const d = after(c, () => {
      throw 'rejected-d';
    });
    const b = after(d, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => {
      throw 'rejected-a';
    });

    return GoogPromise.race([a, b, c, d])
        .then(
            shouldNotCall,
            (value) => {
              assertEquals('rejected-c', value);
              return a;
            })
        .then(shouldNotCall, (reason) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'rejected-a',
              reason);
        });
  },

  testRaceWithRejectBuiltIn() {
    const c = rejectSoon(NativeOrGoogPromise, 'rejected-c');
    const d = after(c, () => {
      throw 'rejected-d';
    });
    const b = after(d, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => {
      throw 'rejected-a';
    });

    return GoogPromise.race([a, b, c, d])
        .then(
            shouldNotCall,
            (value) => {
              assertEquals('rejected-c', value);
              return a;
            })
        .then(shouldNotCall, (reason) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'rejected-a',
              reason);
        });
  },

  testRaceWithRejectAndThrowingThenable() {
    const c = rejectSoon(NativeOrGoogPromise, 'rejected-c');
    const d = new ThrowingThenable('rejected-d');
    const b = CountingThenable.resolve(after(c, () => {
      throw 'rejected-b';
    }));
    const a = GoogPromise.resolve(after(b, () => {
      throw 'rejected-a';
    }));

    return GoogPromise.race([a, b, c, d])
        .then(
            shouldNotCall,
            (value) => {
              assertEquals('rejected-d', value);
              return a;
            })
        .then(shouldNotCall, (reason) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'rejected-a',
              reason);
        });
  },

  testAllWithEmptyList() {
    return GoogPromise.all([]).then((value) => {
      assertArrayEquals([], value);
    });
  },

  testAllWithFulfill() {
    const a = fulfillSoon(GoogPromise, 'a');
    const b = fulfillSoon(GoogPromise, 'b');
    const c = fulfillSoon(GoogPromise, 'c');
    const d = fulfillSoon(GoogPromise, 'd');
    // Test a falsey value.
    const z = fulfillSoon(NativeOrGoogPromise, 0);

    return GoogPromise.all([a, b, c, d, z]).then((value) => {
      assertArrayEquals(['a', 'b', 'c', 'd', 0], value);
    });
  },

  testAllWithThenable() {
    const a = fulfillSoon(GoogPromise, 'a');
    const b = fulfillSoon(CountingThenable, 'b');
    const c = fulfillSoon(GoogPromise, 'c');
    const d = fulfillSoon(GoogPromise, 'd');

    return GoogPromise.all([a, b, c, d]).then((value) => {
      assertArrayEquals(['a', 'b', 'c', 'd'], value);
      // Ensure that the `then` property was only accessed once by
      // `goog.Promise.all`.
      assertEquals(1, b.thenCallCount);
    });
  },

  testAllWithBuiltIn() {
    const a = fulfillSoon(GoogPromise, 'a');
    const b = fulfillSoon(NativeOrGoogPromise, 'b');
    const c = fulfillSoon(GoogPromise, 'c');
    const d = fulfillSoon(GoogPromise, 'd');

    return GoogPromise.all([a, b, c, d]).then((value) => {
      assertArrayEquals(['a', 'b', 'c', 'd'], value);
    });
  },

  testAllWithNonThenable() {
    const a = fulfillSoon(GoogPromise, 'a');
    const b = 'b';
    const c = fulfillSoon(GoogPromise, 'c');
    const d = fulfillSoon(GoogPromise, 'd');

    // Test a falsey value.
    const z = 0;

    return GoogPromise.all([a, b, c, d, z]).then((value) => {
      assertArrayEquals(['a', 'b', 'c', 'd', 0], value);
    });
  },

  testAllWithReject() {
    const a = fulfillSoon(GoogPromise, 'a');
    const b = rejectSoon(GoogPromise, 'rejected-b');
    const c = fulfillSoon(GoogPromise, 'c');
    const d = fulfillSoon(GoogPromise, 'd');


    return GoogPromise.all([a, b, c, d])
        .then(
            shouldNotCall,
            (reason) => {
              assertEquals('rejected-b', reason);
              return a;
            })
        .then((value) => {
          assertEquals(
              'Promise "a" should be fulfilled even though the all()' +
                  'was rejected.',
              'a', value);
        });
  },

  testAllSettledWithEmptyList() {
    return GoogPromise.allSettled([]).then((results) => {
      assertArrayEquals([], results);
    });
  },

  testAllSettledWithFulfillAndReject() {
    const a = fulfillSoon(GoogPromise, 'a');
    const b = rejectSoon(GoogPromise, 'rejected-b');
    const c = 'c';
    const d = rejectSoon(NativeOrGoogPromise, 'rejected-d');
    const e = fulfillSoon(CountingThenable, 'e');
    const f = fulfillSoon(NativeOrGoogPromise, 'f');
    const g = rejectSoon(CountingThenable, 'rejected-g');
    const h = new ThrowingThenable('rejected-h');
    // Test a falsey value.
    const z = 0;

    return GoogPromise.allSettled([a, b, c, d, e, f, g, h, z])
        .then((results) => {
          assertArrayEquals(
              [
                {fulfilled: true, value: 'a'},
                {fulfilled: false, reason: 'rejected-b'},
                {fulfilled: true, value: 'c'},
                {fulfilled: false, reason: 'rejected-d'},
                {fulfilled: true, value: 'e'},
                {fulfilled: true, value: 'f'},
                {fulfilled: false, reason: 'rejected-g'},
                {fulfilled: false, reason: 'rejected-h'},
                {fulfilled: true, value: 0},
              ],
              results);
          // Ensure that the `then` property was only accessed once by
          // `goog.Promise.allSettled`.
          assertEquals(1, e.thenCallCount);
          assertEquals(1, g.thenCallCount);
        });
  },

  testFirstFulfilledWithEmptyList() {
    return GoogPromise.firstFulfilled([]).then((value) => {
      assertUndefined(value);
    });
  },

  testFirstFulfilledWithFulfill() {
    const c = rejectSoon(GoogPromise, 'rejected-c');
    const d = after(c, () => 'd');
    const b = after(d, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => 'a');

    return GoogPromise.firstFulfilled([a, b, c, d])
        .then((value) => {
          assertEquals('d', value);
          return c;
        })
        .then(
            shouldNotCall,
            (reason) => {
              assertEquals(
                  'Promise "c" should be rejected before firstFulfilled() resolves.',
                  'rejected-c', reason);
              return a;
            })
        .then((value) => {
          assertEquals(
              'Promise "a" should be fulfilled after firstFulfilled() resolves.',
              'a', value);
        });
  },

  testFirstFulfilledWithThenables() {
    const c = rejectSoon(CountingThenable, 'rejected-c');
    const d = after(c, () => 'd');
    const b = after(d, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => 'a');

    return GoogPromise.firstFulfilled([a, b, c, d])
        .then((value) => {
          assertEquals('d', value);
          // Ensure that the `then` property was only accessed once by
          // `goog.Promise.firstFulfilled`.
          assertEquals(1, d.thenCallCount);

          return c;
        })
        .then(
            shouldNotCall,
            (reason) => {
              assertEquals(
                  'Thenable "c" should be rejected before firstFulfilled() resolves.',
                  'rejected-c', reason);
              return a;
            })
        .then((value) => {
          assertEquals(
              'Thenable "a" should be fulfilled after firstFulfilled() resolves.',
              'a', value);
        });
  },

  testFirstFulfilledWithBuiltIns() {
    const c = rejectSoon(NativeOrGoogPromise, 'rejected-c');
    const d = after(c, () => 'd');
    const b = after(d, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => 'a');

    return GoogPromise.firstFulfilled([a, b, c, d])
        .then((value) => {
          assertEquals('d', value);
          return c;
        })
        .then(
            shouldNotCall,
            (reason) => {
              assertEquals(
                  'Promise "c" should be rejected before firstFulfilled() resolves.',
                  'rejected-c', reason);
              return a;
            })
        .then((value) => {
          assertEquals(
              'Promise "a" should be fulfilled after firstFulfilled() resolves.',
              'a', value);
        });
  },

  testFirstFulfilledWithNonThenable() {
    const c = rejectSoon(GoogPromise, 'rejected-c');
    const d = 'd';
    const b = after(c, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => 'a');

    return GoogPromise.firstFulfilled([a, b, c, d])
        .then((value) => {
          assertEquals('d', value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testFirstFulfilledWithFalseyNonThenable() {
    const c = rejectSoon(GoogPromise, 'rejected-c');
    const d = 0;
    const b = after(c, () => {
      throw 'rejected-b';
    });
    const a = after(b, () => 'a');

    return GoogPromise.firstFulfilled([a, b, c, d])
        .then((value) => {
          assertEquals(0, value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testFirstFulfilledWithFulfilledBeforeNonThenable() {
    const c = rejectSoon(GoogPromise, 'rejected-c');
    const d = 'd';
    const b = GoogPromise.resolve('b');
    const a = after(c, () => 'a');

    return GoogPromise.firstFulfilled([a, b, c, d])
        .then((value) => {
          assertEquals('b', value);
          // Return the slowest input promise to wait for it to complete.
          return a;
        })
        .then((value) => {
          assertEquals(
              'The slowest promise should resolve eventually.', 'a', value);
        });
  },

  testFirstFulfilledWithReject() {
    const c = rejectSoon(NativeOrGoogPromise, 'rejected-c');
    const d = new ThrowingThenable('rejected-d');
    const b = CountingThenable.resolve(after(c, () => {
      throw 'rejected-b';
    }));
    const a = GoogPromise.resolve(after(b, () => {
      throw 'rejected-a';
    }));

    return GoogPromise.firstFulfilled([a, b, c, d])
        .then(shouldNotCall, (reason) => {
          assertArrayEquals(
              ['rejected-a', 'rejected-b', 'rejected-c', 'rejected-d'], reason);
          // Ensure that the `then` property was only accessed once by
          // `goog.Promise.firstFulfilled`.
          assertEquals(1, b.thenCallCount);
        });
  },

  testThenAlwaysWithFulfill() {
    let thenAlwaysCalled = false;
    return GoogPromise.resolve(sentinel)
        .thenAlways(function() {
          assertEquals(
              'thenAlways should have no arguments', 0, arguments.length);
          thenAlwaysCalled = true;
        })
        .then((value) => {
          assertEquals(sentinel, value);
          assertTrue(thenAlwaysCalled);
        });
  },

  testThenAlwaysWithReject() {
    return GoogPromise.reject(sentinel)
        .thenAlways(function() {
          assertEquals(
              'thenAlways should have no arguments', 0, arguments.length);
        })
        .then(shouldNotCall, (err) => {
          assertEquals(sentinel, err);
          return null;
        });
  },

  testThenAlwaysCalledMultipleTimes() {
    const calls = [];

    const p = GoogPromise.resolve(sentinel);
    p.then((value) => {
      assertEquals(sentinel, value);
      calls.push(1);
      return value;
    });
    p.thenAlways(function() {
      assertEquals(0, arguments.length);
      calls.push(2);
      throw new Error('thenAlways throw');
    });
    p.then((value) => {
      assertEquals(
          'Promise result should not mutate after throw from thenAlways.',
          sentinel, value);
      calls.push(3);
    });
    p.thenAlways(() => {
      assertArrayEquals([1, 2, 3], calls);
    });
    p.thenAlways(() => {
      assertEquals(
          'Should be one unhandled exception from the "thenAlways throw".', 1,
          unhandledRejections.getCallCount());
      const rejectionCall = unhandledRejections.popLastCall();
      assertEquals(1, rejectionCall.getArguments().length);
      const err = rejectionCall.getArguments()[0];
      assertEquals('thenAlways throw', err.message);
      assertEquals(null, rejectionCall.getThis());
    });

    return p.thenAlways(() => {
      assertEquals(3, calls.length);
    });
  },

  testContextWithInit() {
    let initContext;
    const p = new GoogPromise(function(resolve, reject) {
      initContext = this;
    }, sentinel);
    assertNotNull(p);
    assertEquals(sentinel, initContext);
  },

  testContextWithInitDefault() {
    if (!SUPPORTS_STRICT_MODE) {
      return;
    }
    let initContext;
    const p = new GoogPromise(function(resolve, reject) {
      initContext = this;
    });
    assertNotNull(p);
    assertEquals(
        'initFunc should default to being called with undefined', undefined,
        initContext);
  },

  testContextWithFulfillment() {
    if (!SUPPORTS_STRICT_MODE) {
      return;
    }
    return GoogPromise.resolve()
        .then(function() {
          assertEquals(
              '"undefined" should be bound if no context is specified.',
              undefined, this);
        })
        .then(
            function() {
              assertEquals(sentinel, this);
            },
            shouldNotCall, sentinel)
        .thenAlways(function() {
          assertEquals(sentinel, this);
        }, sentinel);
  },

  testContextWithRejection() {
    if (!SUPPORTS_STRICT_MODE) {
      return;
    }
    return GoogPromise.reject()
        .then(
            shouldNotCall,
            function() {
              assertEquals(
                  'Call should with undefined when no context is set.',
                  undefined, this);
              throw new Error('Intentional rejection');
            })
        .then(
            shouldNotCall,
            function() {
              assertEquals(sentinel, this);
            },
            sentinel)
        .thenAlways(
            function() {
              assertEquals(sentinel, this);
            },
            sentinel)
        .thenCatch(function() {
          assertEquals(sentinel, this);
        }, sentinel);
  },

  testCancel() {
    const p = new GoogPromise(goog.nullFunction);
    const child = p.then(shouldNotCall, (reason) => {
      assertTrue(reason instanceof GoogPromise.CancellationError);
      assertEquals(
          'cancellation message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);

      // Return a non-Error to resolve the cancellation rejection.
      return null;
    });
    p.cancel('cancellation message');
    return child;
  },

  async testCancelThenCatchIncludesDetailedStack() {
    if (!new Error()['stack']) {
      // e.stack is missing for IE9, IE10, and IE11.
      return;
    }
    // Given.
    const p = new GoogPromise(goog.nullFunction);
    function recurse(depth) {
      if (depth == 0) {
        p.cancel('cancellation message');
      } else {
        // Increase the number of stack frames.
        recurse(depth - 1);
      }
    }
    recurse(20);

    // When.
    const error = await assertRejects(p);

    // Then.
    const stackLines = error['stack'].split('\n');
    // Note: If Error() is created in an asynchronous frame the length is ~5
    // frames (depending on browser). When Error() is created synchronously
    // The frame is much longer and includes the frames from this test.
    assertTrue(stackLines.length > 20);
  },

  testThenVoidCancel() {
    let thenVoidCalled = false;
    const p = new GoogPromise(goog.nullFunction);

    p.thenVoid(shouldNotCall, (reason) => {
      assertTrue(reason instanceof GoogPromise.CancellationError);
      assertEquals(
          'cancellation message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);
      thenVoidCalled = true;
    });

    p.cancel('cancellation message');
    assertFalse(thenVoidCalled);

    return p.thenCatch(() => {
      assertTrue(thenVoidCalled);

      // Return a non-Error to resolve the cancellation rejection.
      return null;
    });
  },

  testCancelAfterResolve() {
    const p = GoogPromise.resolve();
    p.cancel();
    return p.then(null, shouldNotCall);
  },

  testThenVoidCancelAfterResolve() {
    const p = GoogPromise.resolve();
    p.cancel();
    p.thenVoid(null, shouldNotCall);
    return p;
  },

  testCancelAfterReject() {
    const p = GoogPromise.reject(sentinel);
    p.cancel();
    return p.then(shouldNotCall, (reason) => {
      assertEquals(sentinel, reason);
    });
  },

  testThenVoidCancelAfterReject() {
    let thenVoidCalled = false;
    const p = GoogPromise.reject(sentinel);
    p.cancel();

    p.thenVoid(shouldNotCall, (reason) => {
      assertEquals(sentinel, reason);
      thenVoidCalled = true;
    });

    return p.thenCatch(() => {
      assertTrue(thenVoidCalled);
    });
  },

  testCancelPropagation() {
    let cancelError;
    const p = new GoogPromise(goog.nullFunction);

    const p2 =
        p.then(shouldNotCall, (reason) => {
           cancelError = reason;
           assertTrue(reason instanceof GoogPromise.CancellationError);
           assertEquals(
               'parent cancel message',
               /** @type {!GoogPromise.CancellationError} */ (reason).message);
           return sentinel;
         }).then((value) => {
          assertEquals(
              'Child promises should receive the returned value of the parent.',
              sentinel, value);
        }, shouldNotCall);

    const p3 = p.then(shouldNotCall, (reason) => {
      assertEquals(
          'Every onRejected handler should receive the same cancel error.',
          cancelError, reason);
      assertEquals(
          'parent cancel message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);

      // Return a non-Error to resolve the cancellation rejection.
      return null;
    });

    p.cancel('parent cancel message');
    return GoogPromise.all([p2, p3]);
  },

  testThenVoidCancelPropagation() {
    const resolver = GoogPromise.withResolver();
    let toResolveCount = 2;

    const partialResolve = () => {
      if (--toResolveCount == 0) {
        resolver.resolve();
      }
    };

    let cancelError;
    const p = new GoogPromise(goog.nullFunction);

    const p2 = p.then(shouldNotCall, (reason) => {
      cancelError = reason;
      assertTrue(reason instanceof GoogPromise.CancellationError);
      assertEquals(
          'parent cancel message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);
      return sentinel;
    });
    p2.thenVoid((value) => {
      assertEquals(
          'Child promises should receive the returned value of the parent.',
          sentinel, value);
      partialResolve();
    }, shouldNotCall);

    p.thenVoid(shouldNotCall, (reason) => {
      assertEquals(
          'Every onRejected handler should receive the same cancel error.',
          cancelError, reason);
      assertEquals(
          'parent cancel message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);
      partialResolve();
    });

    p.cancel('parent cancel message');
    return resolver.promise;
  },

  testCancelPropagationUpward() {
    let cancelError;
    const cancelCalls = [];
    const parent = new GoogPromise(goog.nullFunction);

    const child = parent.then(shouldNotCall, (reason) => {
      assertTrue(reason instanceof GoogPromise.CancellationError);
      assertEquals(
          'grandChild cancel message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);
      cancelError = reason;
      cancelCalls.push('parent');
    });

    const grandChild = child.then(shouldNotCall, (reason) => {
      assertEquals(
          'Child should receive the same cancel error.', cancelError, reason);
      cancelCalls.push('child');
    });

    const descendant = grandChild.then(shouldNotCall, (reason) => {
      assertEquals(
          'GrandChild should receive the same cancel error.', cancelError,
          reason);
      cancelCalls.push('grandChild');

      assertArrayEquals(
          'Each promise in the hierarchy has a single child, so canceling the ' +
              'grandChild should cancel each ancestor in order.',
          ['parent', 'child', 'grandChild'], cancelCalls);

      // Return a non-Error to resolve the cancellation rejection.
      return null;
    });

    grandChild.cancel('grandChild cancel message');
    return descendant;
  },

  testThenVoidCancelPropagationUpward() {
    let cancelError;
    const cancelCalls = [];
    const parent = new GoogPromise(goog.nullFunction);

    const child = parent.then(shouldNotCall, (reason) => {
      assertTrue(reason instanceof GoogPromise.CancellationError);
      assertEquals(
          'grandChild cancel message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);
      cancelError = reason;
      cancelCalls.push('parent');
    });

    const grandChild = child.then(shouldNotCall, (reason) => {
      assertEquals(
          'Child should receive the same cancel error.', cancelError, reason);
      cancelCalls.push('child');
    });

    grandChild.thenVoid(shouldNotCall, (reason) => {
      assertEquals(
          'GrandChild should receive the same cancel error.', cancelError,
          reason);
      cancelCalls.push('grandChild');
    });

    grandChild.cancel('grandChild cancel message');
    return grandChild.thenCatch((reason) => {
      assertEquals(cancelError, reason);
      assertArrayEquals(
          'Each promise in the hierarchy has a single child, so canceling the ' +
              'grandChild should cancel each ancestor in order.',
          ['parent', 'child', 'grandChild'], cancelCalls);

      // Return a non-Error to resolve the cancellation rejection.
      return null;
    });
  },

  testCancelPropagationUpwardWithMultipleChildren() {
    let cancelError;
    const cancelCalls = [];
    const parent = fulfillSoon(GoogPromise, sentinel);

    parent.then((value) => {
      assertEquals(
          'Non-canceled callbacks should be called after a sibling is canceled.',
          sentinel, value);
    });

    const child = parent.then(shouldNotCall, (reason) => {
      assertTrue(reason instanceof GoogPromise.CancellationError);
      assertEquals(
          'grandChild cancel message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);
      cancelError = reason;
      cancelCalls.push('child');
    });

    const grandChild = child.then(shouldNotCall, (reason) => {
      assertEquals(reason, cancelError);
      cancelCalls.push('grandChild');
    });
    grandChild.cancel('grandChild cancel message');

    return grandChild.then(shouldNotCall, (reason) => {
      assertEquals(reason, cancelError);
      assertArrayEquals(
          'The parent promise has multiple children, so only the child and ' +
              'grandChild should be canceled.',
          ['child', 'grandChild'], cancelCalls);

      // Return a non-Error to resolve the cancellation rejection.
      return null;
    });
  },

  testThenVoidCancelPropagationUpwardWithMultipleChildren() {
    let cancelError;
    const cancelCalls = [];
    const parent = fulfillSoon(GoogPromise, sentinel);

    parent.thenVoid((value) => {
      assertEquals(
          'Non-canceled callbacks should be called after a sibling is canceled.',
          sentinel, value);
    }, shouldNotCall);

    const child = parent.then(shouldNotCall, (reason) => {
      assertTrue(reason instanceof GoogPromise.CancellationError);
      assertEquals(
          'grandChild cancel message',
          /** @type {!GoogPromise.CancellationError} */ (reason).message);
      cancelError = reason;
      cancelCalls.push('child');
    });

    const grandChild = child.then(shouldNotCall, (reason) => {
      assertEquals(reason, cancelError);
      cancelCalls.push('grandChild');
    });
    grandChild.cancel('grandChild cancel message');

    grandChild.thenVoid(shouldNotCall, (reason) => {
      assertEquals(reason, cancelError);
      cancelCalls.push('void grandChild');
    });

    return grandChild.then(shouldNotCall, (reason) => {
      assertEquals(reason, cancelError);
      assertArrayEquals(
          'The parent promise has multiple children, so only the child and ' +
              'grandChildren should be canceled.',
          ['child', 'grandChild', 'void grandChild'], cancelCalls);

      // Return a non-Error to resolve the cancellation rejection.
      return null;
    });
  },

  testCancelRecovery() {
    const cancelCalls = [];

    const parent = fulfillSoon(GoogPromise, sentinel);

    const sibling1 = parent.then((value) => {
      assertEquals(
          'Non-canceled callbacks should be called after a sibling is canceled.',
          sentinel, value);
    });

    const sibling2 = parent.then(shouldNotCall, (reason) => {
      assertTrue(reason instanceof GoogPromise.CancellationError);
      cancelCalls.push('sibling2');
      return sentinel;
    });

    const grandChild = sibling2.then((value) => {
      cancelCalls.push('child');
      assertEquals(
          'Returning a non-cancel value should uncancel the grandChild.', value,
          sentinel);
      assertArrayEquals(['sibling2', 'child'], cancelCalls);
    }, shouldNotCall);

    grandChild.cancel();
    return GoogPromise.all([sibling1, grandChild]);
  },

  testCancellationError() {
    const err = new GoogPromise.CancellationError('cancel message');
    assertTrue(err instanceof Error);
    assertTrue(err instanceof GoogPromise.CancellationError);
    assertFalse(err.reportErrorToServer);
    assertEquals('cancel', err.name);
    assertEquals('cancel message', err.message);
  },

  testMockClock() {
    mockClock.install();

    let resolveA;
    let resolveB;
    const calls = [];

    const p = new GoogPromise((resolve, reject) => {
      resolveA = resolve;
    });

    p.then((value) => {
      assertEquals(sentinel, value);
      calls.push('then');
    });

    const fulfilledChild = p.then((value) => {
                              assertEquals(sentinel, value);
                              return GoogPromise.resolve(1);
                            }).then((value) => {
      assertEquals(1, value);
      calls.push('fulfilledChild');
    });
    assertNotNull(fulfilledChild);

    const rejectedChild = p.then((value) => {
                             assertEquals(sentinel, value);
                             return GoogPromise.reject(2);
                           }).then(shouldNotCall, (reason) => {
      assertEquals(2, reason);
      calls.push('rejectedChild');
    });
    assertNotNull(rejectedChild);

    const unresolvedChild = p.then((value) => {
                               assertEquals(sentinel, value);
                               return new GoogPromise((r) => {
                                 resolveB = r;
                               });
                             }).then((value) => {
      assertEquals(3, value);
      calls.push('unresolvedChild');
    });
    assertNotNull(unresolvedChild);

    resolveA(sentinel);
    assertArrayEquals(
        'Calls must not be resolved until the clock ticks.', [], calls);

    mockClock.tick();
    assertArrayEquals(
        'All resolved Promises should execute in the same timestep.',
        ['then', 'fulfilledChild', 'rejectedChild'], calls);

    resolveB(3);
    assertArrayEquals(
        'New calls must not resolve until the clock ticks.',
        ['then', 'fulfilledChild', 'rejectedChild'], calls);

    mockClock.tick();
    assertArrayEquals(
        'All callbacks should have executed.',
        ['then', 'fulfilledChild', 'rejectedChild', 'unresolvedChild'], calls);
  },

  testHandledRejection() {
    mockClock.install();
    GoogPromise.reject(sentinel).then(shouldNotCall, (reason) => {});

    mockClock.tick();
    assertEquals(0, unhandledRejections.getCallCount());
  },

  testThenVoidHandledRejection() {
    mockClock.install();
    GoogPromise.reject(sentinel).thenVoid(shouldNotCall, (reason) => {});

    mockClock.tick();
    assertEquals(0, unhandledRejections.getCallCount());
  },

  testUnhandledRejection1() {
    mockClock.install();
    GoogPromise.reject(sentinel);

    mockClock.tick();
    assertEquals(1, unhandledRejections.getCallCount());
    const rejectionCall = unhandledRejections.popLastCall();
    assertArrayEquals([sentinel], rejectionCall.getArguments());
    assertEquals(null, rejectionCall.getThis());
  },

  testUnhandledRejection2() {
    mockClock.install();
    GoogPromise.reject(sentinel).then(shouldNotCall);

    mockClock.tick();
    assertEquals(1, unhandledRejections.getCallCount());
    const rejectionCall = unhandledRejections.popLastCall();
    assertArrayEquals([sentinel], rejectionCall.getArguments());
    assertEquals(null, rejectionCall.getThis());
  },

  testThenVoidUnhandledRejection() {
    mockClock.install();
    GoogPromise.reject(sentinel).thenVoid(shouldNotCall);

    mockClock.tick();
    assertEquals(1, unhandledRejections.getCallCount());
    const rejectionCall = unhandledRejections.popLastCall();
    assertArrayEquals([sentinel], rejectionCall.getArguments());
    assertEquals(null, rejectionCall.getThis());
  },

  testUnhandledRejection() {
    const resolver = GoogPromise.withResolver();

    GoogPromise.setUnhandledRejectionHandler((err) => {
      assertEquals(sentinel, err);
      resolver.resolve();
    });
    GoogPromise.reject(sentinel);

    return resolver.promise;
  },

  testUnhandledThrow() {
    const resolver = GoogPromise.withResolver();

    GoogPromise.setUnhandledRejectionHandler((err) => {
      assertEquals(sentinel, err);
      resolver.resolve();
    });
    GoogPromise.resolve().then(() => {
      throw sentinel;
    });

    return resolver.promise;
  },

  testThenVoidUnhandledThrow() {
    const resolver = GoogPromise.withResolver();

    GoogPromise.setUnhandledRejectionHandler((error) => {
      assertEquals(sentinel, error);
      resolver.resolve();
    });

    GoogPromise.resolve().thenVoid(() => {
      throw sentinel;
    });

    return resolver.promise;
  },

  testUnhandledBlockingRejection() {
    mockClock.install();
    const blocker = GoogPromise.reject(sentinel);
    GoogPromise.resolve(blocker);

    mockClock.tick();
    assertEquals(1, unhandledRejections.getCallCount());
    const rejectionCall = unhandledRejections.popLastCall();
    assertArrayEquals([sentinel], rejectionCall.getArguments());
    assertEquals(null, rejectionCall.getThis());
  },

  testUnhandledRejectionAfterThenAlways() {
    mockClock.install();
    const resolver = GoogPromise.withResolver();
    resolver.promise.thenAlways(() => {});
    resolver.reject(sentinel);

    mockClock.tick();
    assertEquals(1, unhandledRejections.getCallCount());
    const rejectionCall = unhandledRejections.popLastCall();
    assertArrayEquals([sentinel], rejectionCall.getArguments());
    assertEquals(null, rejectionCall.getThis());
  },

  testHandledBlockingRejection() {
    mockClock.install();
    const blocker = GoogPromise.reject(sentinel);
    GoogPromise.resolve(blocker).then(shouldNotCall, (reason) => {});

    mockClock.tick();
    assertEquals(0, unhandledRejections.getCallCount());
  },

  testThenVoidHandledBlockingRejection() {
    const shouldCall = recordFunction();

    mockClock.install();
    const blocker = GoogPromise.reject(sentinel);
    GoogPromise.resolve(blocker).thenVoid(shouldNotCall, shouldCall);

    mockClock.tick();
    assertEquals(0, unhandledRejections.getCallCount());
    assertEquals(1, shouldCall.getCallCount());
  },

  testUnhandledRejectionWithTimeout() {
    mockClock.install();
    stubs.replace(GoogPromise, 'UNHANDLED_REJECTION_DELAY', 200);
    GoogPromise.reject(sentinel);

    mockClock.tick(199);
    assertEquals(0, unhandledRejections.getCallCount());

    mockClock.tick(1);
    assertEquals(1, unhandledRejections.getCallCount());
  },

  testHandledRejectionWithTimeout() {
    mockClock.install();
    stubs.replace(GoogPromise, 'UNHANDLED_REJECTION_DELAY', 200);
    const p = GoogPromise.reject(sentinel);

    mockClock.tick(199);
    p.then(shouldNotCall, (reason) => {});

    mockClock.tick(1);
    assertEquals(0, unhandledRejections.getCallCount());
  },

  testUnhandledRejectionDisabled() {
    mockClock.install();
    stubs.replace(GoogPromise, 'UNHANDLED_REJECTION_DELAY', -1);
    GoogPromise.reject(sentinel);

    mockClock.tick();
    assertEquals(0, unhandledRejections.getCallCount());
  },

  async testUnhandledRejectionNotFiredWhenAwaitingARejectedGoogPromise() {
    try {
      await GoogPromise.reject(sentinel);
    } catch (e) {
      // Expected
    }

    // TODO(user): Expect 0 unhandled rejections in all environemnts.
    assertEquals(MICROTASKS_EXIST ? 1 : 0, unhandledRejections.getCallCount());
  },

  testThenableInterface() {
    const promise = new GoogPromise((resolve, reject) => {});
    assertTrue(Thenable.isImplementedBy(promise));

    assertFalse(Thenable.isImplementedBy({}));
    assertFalse(Thenable.isImplementedBy('string'));
    assertFalse(Thenable.isImplementedBy(1));
    assertFalse(Thenable.isImplementedBy({then: function() {}}));

    /** @constructor */
    function T() {}
    T.prototype.then = (opt_a, opt_b, opt_c) => {};
    Thenable.addImplementation(T);
    assertTrue(Thenable.isImplementedBy(new T));

    // Test COMPILED code path.
    try {
      globalThis['COMPILED'] = true;
      /** @constructor */
      function C() {}
      C.prototype.then = (opt_a, opt_b, opt_c) => {};
      Thenable.addImplementation(C);
      assertTrue(Thenable.isImplementedBy(new C));
    } finally {
      globalThis['COMPILED'] = false;
    }
  },

  testCreateWithResolver_Resolved() {
    mockClock.install();
    let timesCalled = 0;

    const resolver = GoogPromise.withResolver();

    resolver.promise.then((value) => {
      timesCalled++;
      assertEquals(sentinel, value);
    }, fail);

    assertEquals(
        'then() must return before callbacks are invoked.', 0, timesCalled);

    mockClock.tick();

    assertEquals(
        'promise is not resolved until resolver is invoked.', 0, timesCalled);

    resolver.resolve(sentinel);

    assertEquals('resolution is delayed until the next tick', 0, timesCalled);

    mockClock.tick();

    assertEquals('onFulfilled must be called exactly once.', 1, timesCalled);
  },

  testCreateWithResolver_Rejected() {
    mockClock.install();
    let timesCalled = 0;

    const resolver = GoogPromise.withResolver();

    resolver.promise.then(fail, (reason) => {
      timesCalled++;
      assertEquals(sentinel, reason);
    });

    assertEquals(
        'then() must return before callbacks are invoked.', 0, timesCalled);

    mockClock.tick();

    assertEquals(
        'promise is not resolved until resolver is invoked.', 0, timesCalled);

    resolver.reject(sentinel);

    assertEquals('resolution is delayed until the next tick', 0, timesCalled);

    mockClock.tick();

    assertEquals('onFulfilled must be called exactly once.', 1, timesCalled);
  },

  /** @suppress {visibility} */
  testLinksBetweenParentsAndChildrenAreCutOnResolve() {
    mockClock.install();
    const parentResolver = GoogPromise.withResolver();
    const parent = parentResolver.promise;
    const child = parent.then(() => {});
    assertNotNull(child.parent_);
    assertEquals(null, parent.callbackEntries_.next);
    parentResolver.resolve();
    mockClock.tick();
    assertNull(child.parent_);
    assertEquals(null, parent.callbackEntries_);
  },

  /** @suppress {visibility} */
  testLinksBetweenParentsAndChildrenAreCutWithUnresolvedChild() {
    mockClock.install();
    const parentResolver = GoogPromise.withResolver();
    const parent = parentResolver.promise;
    const child = parent.then(() => {
      // Will never resolve.
      return new GoogPromise(() => {});
    });
    assertNotNull(child.parent_);
    assertNull(parent.callbackEntries_.next);
    parentResolver.resolve();
    mockClock.tick();
    assertNull(child.parent_);
    assertEquals(null, parent.callbackEntries_);
  },

  /** @suppress {visibility} */
  testLinksBetweenParentsAndChildrenAreCutOnCancel() {
    mockClock.install();
    const parent = new GoogPromise(() => {});
    const child = parent.then(() => {});
    const grandChild = child.then(() => {});
    assertEquals(null, child.callbackEntries_.next);
    assertNotNull(child.parent_);
    assertEquals(null, parent.callbackEntries_.next);
    parent.cancel();
    mockClock.tick();
    assertNull(child.parent_);
    assertNull(grandChild.parent_);
    assertEquals(null, parent.callbackEntries_);
    assertEquals(null, child.callbackEntries_);
  },
});
