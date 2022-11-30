/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/** @fileoverview Unit tests for functions. */

goog.module('goog.functionsTest');
goog.setTestOnly();

const MockClock = goog.require('goog.testing.MockClock');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const functions = goog.require('goog.functions');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

const fTrue = makeCallOrderLogger('fTrue', true);
const gFalse = makeCallOrderLogger('gFalse', false);
const hTrue = makeCallOrderLogger('hTrue', true);

const stubs = new PropertyReplacer();
let callOrder = null;

const foo = 'global';
const obj = {
  foo: 'obj'
};

/** @suppress {globalThis} suppression added to enable type checking */
function getFoo(arg1, arg2) {
  return {foo: this.foo, arg1: arg1, arg2: arg2};
}

function makeCallOrderLogger(name, returnValue) {
  return () => {
    callOrder.push(name);
    return returnValue;
  };
}

function assertCallOrderAndReset(expectedArray) {
  assertArrayEquals(expectedArray, callOrder);
  callOrder = [];
}

/**
 * Wraps a `recordFunction` with the specified decorator and
 * executes a list of command sequences, asserting that in each case the
 * decorated function is called the expected number of times.
 * @param {function():*} decorator The async decorator to test.
 * @param {!Object<string,number>} expectedCommandSequenceCalls An object
 *     mapping string command sequences (where 'f' is 'fire' and 'w' is 'wait')
 *     to the number times we expect a decorated function to be called during
 *     the execution of those commands.
 * @suppress {checkTypes} suppression added to enable type checking
 */
function assertAsyncDecoratorCommandSequenceCalls(
    decorator, expectedCommandSequenceCalls) {
  const interval = 500;

  const mockClock = new MockClock(true);
  for (let commandSequence in expectedCommandSequenceCalls) {
    const recordedFunction = recordFunction();
    /** @suppress {checkTypes} suppression added to enable type checking */
    const f = decorator(recordedFunction, interval);

    for (let i = 0; i < commandSequence.length; ++i) {
      switch (commandSequence[i]) {
        case 'f':
          f();
          break;
        case 'w':
          mockClock.tick(interval);
          break;
      }
    }

    const expectedCalls = expectedCommandSequenceCalls[commandSequence];
    assertEquals(
        `Expected ${expectedCalls} calls for command sequence "` +
            commandSequence + '" (' +
            Array.prototype.map
                .call(
                    commandSequence,
                    command => {
                      switch (command) {
                        case 'f':
                          return 'fire';
                        case 'w':
                          return 'wait';
                      }
                    })
                .join(' -> ') +
            ')',
        expectedCalls, recordedFunction.getCallCount());
  }
  mockClock.uninstall();
}

testSuite({
  setUp() {
    callOrder = [];
  },

  tearDown() {
    stubs.reset();
  },

  testTrue() {
    assertTrue(functions.TRUE());
  },

  testFalse() {
    assertFalse(functions.FALSE());
  },

  testLock() {
    function add(var_args) {
      let result = 0;
      for (let i = 0; i < arguments.length; i++) {
        result += arguments[i];
      }
      return result;
    }

    assertEquals(6, add(1, 2, 3));
    assertEquals(0, functions.lock(add)(1, 2, 3));
    assertEquals(3, functions.lock(add, 2)(1, 2, 3));
    assertEquals(6, goog.partial(add, 1, 2)(3));
    assertEquals(3, functions.lock(goog.partial(add, 1, 2))(3));
  },

  testNth() {
    assertEquals(1, functions.nth(0)(1));
    assertEquals(2, functions.nth(1)(1, 2));
    assertEquals('a', functions.nth(0)('a', 'b'));
    assertEquals(undefined, functions.nth(0)());
    assertEquals(undefined, functions.nth(1)(true));
    assertEquals(undefined, functions.nth(-1)());
  },

  testPartialRight() {
    const f = (x, y) => x / y;
    const g = functions.partialRight(f, 2);
    assertEquals(2, g(4));

    const h = functions.partialRight(f, 4, 2);
    assertEquals(2, h());

    const i = functions.partialRight(f);
    assertEquals(2, i(4, 2));
  },

  testPartialRightFreeFunction() {
    const f = function(x, y) {
      assertUndefined(this);
      return x / y;
    };
    const g = functions.partialRight(f, 2);
    const h = functions.partialRight(g, 4);
    assertEquals(2, h());
  },

  testPartialRightWithCall() {
    const obj = {};
    const f = function(x, y) {
      assertEquals(obj, this);
      return x / y;
    };
    const g = functions.partialRight(f, 2);
    const h = functions.partialRight(g, 4);
    assertEquals(2, h.call(obj));
  },

  testPartialRightAndBind() {
    // This ensures that this "survives" through a partialRight.
    const p = functions.partialRight(getFoo, 'dog');
    const b = goog.bind(p, obj, 'hot');

    const res = b();
    assertEquals(obj.foo, res.foo);
    assertEquals('hot', res.arg1);
    assertEquals('dog', res.arg2);
  },

  testBindAndPartialRight() {
    // This ensures that this "survives" through a partialRight.
    const b = goog.bind(getFoo, obj, 'hot');
    const p = functions.partialRight(b, 'dog');

    const res = p();
    assertEquals(obj.foo, res.foo);
    assertEquals('hot', res.arg1);
    assertEquals('dog', res.arg2);
  },

  testPartialRightMultipleCalls() {
    const f = recordFunction();

    const a = functions.partialRight(f, 'foo');
    const b = functions.partialRight(a, 'bar');

    a();
    a();
    b();
    b();

    assertEquals(4, f.getCallCount());

    const calls = f.getCalls();
    assertArrayEquals(['foo'], calls[0].getArguments());
    assertArrayEquals(['foo'], calls[1].getArguments());
    assertArrayEquals(['bar', 'foo'], calls[2].getArguments());
    assertArrayEquals(['bar', 'foo'], calls[3].getArguments());
  },

  testIdentity() {
    assertEquals(3, functions.identity(3));
    assertEquals(3, functions.identity(3, 4, 5, 6));
    assertEquals('Hi there', functions.identity('Hi there'));
    assertEquals(null, functions.identity(null));
    assertEquals(undefined, functions.identity());

    const arr = [1, 'b', null];
    assertEquals(arr, functions.identity(arr));
    const obj = {a: 'ay', b: 'bee', c: 'see'};
    assertEquals(obj, functions.identity(obj));
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testConstant() {
    assertEquals(3, functions.constant(3)());
    assertEquals(undefined, functions.constant()());
  },

  testError() {
    const f = functions.error('x');
    const e = assertThrows(
        'A function created by goog.functions.error must throw an error', f);
    assertEquals('x', e.message);
  },

  testFail() {
    const obj = {};
    const f = functions.fail(obj);
    const e = assertThrows(
        'A function created by goog.functions.raise must throw its input', f);
    assertEquals(obj, e);
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testCompose() {
    const add2 = (x) => x + 2;

    const doubleValue = (x) => x * 2;

    assertEquals(6, functions.compose(doubleValue, add2)(1));
    assertEquals(4, functions.compose(add2, doubleValue)(1));
    assertEquals(6, functions.compose(add2, add2, doubleValue)(1));
    assertEquals(
        12, functions.compose(doubleValue, add2, add2, doubleValue)(1));
    assertUndefined(functions.compose()(1));
    assertEquals(3, functions.compose(add2)(1));

    const add2Numbers = (x, y) => x + y;
    assertEquals(17, functions.compose(add2Numbers)(10, 7));
    assertEquals(34, functions.compose(doubleValue, add2Numbers)(10, 7));
  },

  testAdd() {
    assertUndefined(functions.sequence()());
    assertCallOrderAndReset([]);

    assert(functions.sequence(fTrue)());
    assertCallOrderAndReset(['fTrue']);

    assertFalse(functions.sequence(fTrue, gFalse)());
    assertCallOrderAndReset(['fTrue', 'gFalse']);

    assert(functions.sequence(fTrue, gFalse, hTrue)());
    assertCallOrderAndReset(['fTrue', 'gFalse', 'hTrue']);

    assert(functions.sequence(functions.identity)(true));
    assertFalse(functions.sequence(functions.identity)(false));
  },

  testAnd() {
    // the return value is unspecified for an empty and
    functions.and()();
    assertCallOrderAndReset([]);

    assert(functions.and(fTrue)());
    assertCallOrderAndReset(['fTrue']);

    assertFalse(functions.and(fTrue, gFalse)());
    assertCallOrderAndReset(['fTrue', 'gFalse']);

    assertFalse(functions.and(fTrue, gFalse, hTrue)());
    assertCallOrderAndReset(['fTrue', 'gFalse']);

    assert(functions.and(functions.identity)(true));
    assertFalse(functions.and(functions.identity)(false));
  },

  testOr() {
    // the return value is unspecified for an empty or
    functions.or()();
    assertCallOrderAndReset([]);

    assert(functions.or(fTrue)());
    assertCallOrderAndReset(['fTrue']);

    assert(functions.or(fTrue, gFalse)());
    assertCallOrderAndReset(['fTrue']);

    assert(functions.or(fTrue, gFalse, hTrue)());
    assertCallOrderAndReset(['fTrue']);

    assert(functions.or(functions.identity)(true));
    assertFalse(functions.or(functions.identity)(false));
  },

  testNot() {
    assertTrue(functions.not(gFalse)());
    assertCallOrderAndReset(['gFalse']);

    assertTrue(functions.not(functions.identity)(false));
    assertFalse(functions.not(functions.identity)(true));

    const f = (a, b) => {
      assertEquals(1, a);
      assertEquals(2, b);
      return false;
    };

    assertTrue(functions.not(f)(1, 2));
  },

  testCreate(expectedArray) {
    const tempConstructor = function(a, b) {
      this.foo = a;
      this.bar = b;
    };

    const factory = goog.partial(functions.create, tempConstructor, 'baz');
    const instance = factory('qux');

    assert(instance instanceof tempConstructor);
    assertEquals(instance.foo, 'baz');
    assertEquals(instance.bar, 'qux');
  },

  testWithReturnValue() {
    const obj = {};
    const f = function(a, b) {
      assertEquals(obj, this);
      assertEquals(1, a);
      assertEquals(2, b);
    };
    assertTrue(functions.withReturnValue(f, true).call(obj, 1, 2));
    assertFalse(functions.withReturnValue(f, false).call(obj, 1, 2));
  },

  testEqualTo() {
    assertTrue(functions.equalTo(42)(42));
    assertFalse(functions.equalTo(42)(13));
    assertFalse(functions.equalTo(42)('a string'));

    assertFalse(functions.equalTo(42)('42'));
    assertTrue(functions.equalTo(42, true)('42'));

    assertTrue(functions.equalTo(0)(0));
    assertFalse(functions.equalTo(0)(''));
    assertFalse(functions.equalTo(0)(1));

    assertTrue(functions.equalTo(0, true)(0));
    assertTrue(functions.equalTo(0, true)(''));
    assertFalse(functions.equalTo(0, true)(1));
  },

  testCacheReturnValue() {
    const returnFive = () => 5;

    const recordedReturnFive = recordFunction(returnFive);
    const cachedRecordedReturnFive =
        functions.cacheReturnValue(recordedReturnFive);

    assertEquals(0, recordedReturnFive.getCallCount());
    assertEquals(5, cachedRecordedReturnFive());
    assertEquals(1, recordedReturnFive.getCallCount());
    assertEquals(5, cachedRecordedReturnFive());
    assertEquals(1, recordedReturnFive.getCallCount());
  },

  testCacheReturnValueFlagEnabled() {
    let count = 0;
    const returnIncrementingInteger = () => {
      count++;
      return count;
    };

    const recordedFunction = recordFunction(returnIncrementingInteger);
    const cachedRecordedFunction = functions.cacheReturnValue(recordedFunction);

    assertEquals(0, recordedFunction.getCallCount());
    assertEquals(1, cachedRecordedFunction());
    assertEquals(1, recordedFunction.getCallCount());
    assertEquals(1, cachedRecordedFunction());
    assertEquals(1, recordedFunction.getCallCount());
    assertEquals(1, cachedRecordedFunction());
  },

  testCacheReturnValueFlagDisabled() {
    stubs.set(functions, 'CACHE_RETURN_VALUE', false);

    let count = 0;
    const returnIncrementingInteger = () => {
      count++;
      return count;
    };

    const recordedFunction = recordFunction(returnIncrementingInteger);
    const cachedRecordedFunction = functions.cacheReturnValue(recordedFunction);

    assertEquals(0, recordedFunction.getCallCount());
    assertEquals(1, cachedRecordedFunction());
    assertEquals(1, recordedFunction.getCallCount());
    assertEquals(2, cachedRecordedFunction());
    assertEquals(2, recordedFunction.getCallCount());
    assertEquals(3, cachedRecordedFunction());
  },

  testOnce() {
    const recordedFunction = recordFunction();
    const f = functions.once(recordedFunction);

    assertEquals(0, recordedFunction.getCallCount());
    f();
    assertEquals(1, recordedFunction.getCallCount());
    f();
    assertEquals(1, recordedFunction.getCallCount());
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testDebounce() {
    // Encoded sequences of commands to perform mapped to expected # of calls.
    //   f: fire
    //   w: wait (for the timer to elapse)
    assertAsyncDecoratorCommandSequenceCalls(functions.debounce, {
      'f': 0,
      'ff': 0,
      'fff': 0,
      'fw': 1,
      'ffw': 1,
      'fffw': 1,
      'fwffwf': 2,
      'ffwwwffwwfwf': 3,
    });
  },

  testDebounceScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'y': 0};
    functions.debounce(function() {
      ++this['y'];
    }, interval, x)();
    assertEquals(0, x['y']);

    mockClock.tick(interval);
    assertEquals(1, x['y']);

    mockClock.uninstall();
  },

  testDebounceArgumentBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    let calls = 0;
    const debouncedFn = functions.debounce((a, b, c) => {
      ++calls;
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval);

    debouncedFn(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(1, calls);

    // goog.functions.debounce should always pass the last arguments passed to
    // the decorator into the decorated function, even if called multiple times.
    debouncedFn();
    mockClock.tick(interval / 2);
    debouncedFn(8, null, true);
    debouncedFn(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, calls);

    mockClock.uninstall();
  },

  testDebounceArgumentAndScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'calls': 0};
    const debouncedFn = functions.debounce(function(a, b, c) {
      ++this['calls'];
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval, x);

    debouncedFn(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(1, x['calls']);

    // goog.functions.debounce should always pass the last arguments passed to
    // the decorator into the decorated function, even if called multiple times.
    debouncedFn();
    mockClock.tick(interval / 2);
    debouncedFn(8, null, true);
    debouncedFn(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, x['calls']);

    mockClock.uninstall();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testThrottle() {
    // Encoded sequences of commands to perform mapped to expected # of calls.
    //   f: fire
    //   w: wait (for the timer to elapse)
    assertAsyncDecoratorCommandSequenceCalls(functions.throttle, {
      'f': 1,
      'ff': 1,
      'fff': 1,
      'fw': 1,
      'ffw': 2,
      'fwf': 2,
      'fffw': 2,
      'fwfff': 2,
      'fwfffw': 3,
      'fwffwf': 3,
      'ffwf': 2,
      'ffwff': 2,
      'ffwfw': 3,
      'ffwffwf': 3,
      'ffwffwff': 3,
      'ffwffwffw': 4,
      'ffwwwffwwfw': 5,
      'ffwwwffwwfwf': 6,
    });
  },

  testThrottleScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'y': 0};
    functions.throttle(function() {
      ++this['y'];
    }, interval, x)();
    assertEquals(1, x['y']);

    mockClock.uninstall();
  },

  testThrottleArgumentBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    let calls = 0;
    const throttledFn = functions.throttle((a, b, c) => {
      ++calls;
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval);

    throttledFn(3, 'string', false);
    assertEquals(1, calls);

    // goog.functions.throttle should always pass the last arguments passed to
    // the decorator into the decorated function, even if called multiple times.
    throttledFn();
    mockClock.tick(interval / 2);
    throttledFn(8, null, true);
    throttledFn(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, calls);

    mockClock.uninstall();
  },

  testThrottleArgumentAndScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'calls': 0};
    const throttledFn = functions.throttle(function(a, b, c) {
      ++this['calls'];
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval, x);

    throttledFn(3, 'string', false);
    assertEquals(1, x['calls']);

    // goog.functions.throttle should always pass the last arguments passed to
    // the decorator into the decorated function, even if called multiple times.
    throttledFn();
    mockClock.tick(interval / 2);
    throttledFn(8, null, true);
    throttledFn(3, 'string', false);
    mockClock.tick(interval);
    assertEquals(2, x['calls']);

    mockClock.uninstall();
  },

  /** @suppress {checkTypes} suppression added to enable type checking */
  testRateLimit() {
    // Encoded sequences of commands to perform mapped to expected # of calls.
    //   f: fire
    //   w: wait (for the timer to elapse)
    assertAsyncDecoratorCommandSequenceCalls(functions.rateLimit, {
      'f': 1,
      'ff': 1,
      'fff': 1,
      'fw': 1,
      'ffw': 1,
      'fwf': 2,
      'fffw': 1,
      'fwfff': 2,
      'fwfffw': 2,
      'fwffwf': 3,
      'ffwf': 2,
      'ffwff': 2,
      'ffwfw': 2,
      'ffwffwf': 3,
      'ffwffwff': 3,
      'ffwffwffw': 3,
      'ffwwwffwwfw': 3,
      'ffwwwffwwfwf': 4,
    });
  },

  testRateLimitScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'y': 0};
    functions.rateLimit(function() {
      ++this['y'];
    }, interval, x)();
    assertEquals(1, x['y']);

    mockClock.uninstall();
  },

  testRateLimitArgumentBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    let calls = 0;
    const rateLimitedFn = functions.rateLimit((a, b, c) => {
      ++calls;
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval);

    rateLimitedFn(3, 'string', false);
    assertEquals(1, calls);

    // goog.functions.rateLimit should always pass the first arguments passed to
    // the
    // decorator into the decorated function, even if called multiple times.
    rateLimitedFn();
    mockClock.tick(interval / 2);
    rateLimitedFn(8, null, true);
    mockClock.tick(interval);
    rateLimitedFn(3, 'string', false);
    assertEquals(2, calls);

    mockClock.uninstall();
  },

  testRateLimitArgumentAndScopeBinding() {
    const interval = 500;
    const mockClock = new MockClock(true);

    const x = {'calls': 0};
    const rateLimitedFn = functions.rateLimit(function(a, b, c) {
      ++this['calls'];
      assertEquals(3, a);
      assertEquals('string', b);
      assertEquals(false, c);
    }, interval, x);

    rateLimitedFn(3, 'string', false);
    assertEquals(1, x['calls']);

    // goog.functions.rateLimit should always pass the last arguments passed to
    // the
    // decorator into the decorated function, even if called multiple times.
    rateLimitedFn();
    mockClock.tick(interval / 2);
    rateLimitedFn(8, null, true);
    mockClock.tick(interval);
    rateLimitedFn(3, 'string', false);
    assertEquals(2, x['calls']);

    mockClock.uninstall();
  },

  testIsFunction() {
    assertTrue(functions.isFunction(() => {}));
    assertTrue(functions.isFunction(function() {}));
    assertTrue(functions.isFunction(class {}));
    assertTrue(functions.isFunction(function*() {}));
    assertTrue(functions.isFunction(async function() {}));
    assertFalse(functions.isFunction(0));
    assertFalse(functions.isFunction(false));
    assertFalse(functions.isFunction(''));
    assertFalse(functions.isFunction({}));
    assertFalse(functions.isFunction([]));
  }
});
