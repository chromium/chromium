/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.Environment');
goog.module.declareLegacyNamespace();

const DebugConsole = goog.require('goog.debug.Console');
const MockClock = goog.require('goog.testing.MockClock');
const MockControl = goog.require('goog.testing.MockControl');
const PropertyReplacer = goog.require('goog.testing.PropertyReplacer');
const TestCase = goog.require('goog.testing.TestCase');
const Thenable = goog.require('goog.Thenable');
const asserts = goog.require('goog.asserts');

/** @suppress {extraRequire} Declares globals */
goog.require('goog.testing.jsunit');


/**
 * JsUnit environments allow developers to customize the existing testing
 * lifecycle by hitching additional setUp and tearDown behaviors to tests.
 *
 * Environments will run their setUp steps in the order in which they
 * are instantiated and registered. During tearDown, the environments will
 * unwind the setUp and execute in reverse order.
 *
 * See http://go/jsunit-env for more information.
 */
class Environment {
  constructor() {
    // Use the same EnvironmentTestCase instance across all Environment objects.
    if (!Environment.activeTestCase_) {
      const testcase = new EnvironmentTestCase();

      Environment.activeTestCase_ = testcase;
    }
    Environment.activeTestCase_.registerEnvironment_(this);

    /**
     * Mocks are not type-checkable. To reduce burden on tests that are type
     * checked, this is typed as "?" to turn off JSCompiler checking.
     * TODO(user): Enable a type-checked mocking library.
     * @type {?}
     */
    this.mockControl = null;

    /** @type {?MockClock} */
    this.mockClock = null;

    /** @private {boolean} */
    this.shouldMakeMockControl_ = false;

    /** @protected {boolean} */
    this.mockClockOn = false;

    /** @const {!DebugConsole} */
    this.console = Environment.console_;

    /** @const {!PropertyReplacer} */
    this.replacer = new PropertyReplacer();
  }

  /**
   * Runs immediately before the setUpPage phase of JsUnit tests.
   * @return {!IThenable<*>|undefined} An optional Promise which must be
   *     resolved before the test is executed.
   */
  setUpPage() {
    if (this.mockClockOn && !this.hasMockClock()) {
      this.mockClock = new MockClock(true);
    }
  }

  /** Runs immediately after the tearDownPage phase of JsUnit tests. */
  tearDownPage() {
    // If we created the mockClock, we'll also dispose it.
    if (this.hasMockClock()) {
      this.mockClock.dispose();
    }
  }

  /**
   * Runs immediately before the setUp phase of JsUnit tests.
   * @return {!IThenable<*>|undefined} An optional Promise which must be
   *     resolved before the test case is executed.
   */
  setUp() {}

  /**
   * Runs immediately after the tearDown phase of JsUnit tests.
   * @return {!IThenable<*>|undefined} An optional Promise which must be
   *     resolved before the next test case is executed.
   */
  tearDown() {
    // Make sure promises and other stuff that may still be scheduled,
    // get a
    // chance to run (and throw errors).
    if (this.mockClock) {
      for (let i = 0; i < 100; i++) {
        this.mockClock.tick(1000);
      }
      // If we created the mockClock, we'll also reset it.
      if (this.hasMockClock()) {
        this.mockClock.reset();
      }
    }
    // Reset all changes made by the PropertyReplacer.
    this.replacer.reset();
    // Make sure the user did not forget to call $replayAll & $verifyAll
    // in their test. This is a noop if they did. This is important
    // because:
    // - Engineers thinks that not all their tests need to replay and
    // verify.
    //   That lets tests sneak in that call mocks but never replay those
    //   calls.
    // - Then some well meaning maintenance engineer wants to update the
    // test
    //   with some new mock, adds a replayAll and BOOM the test fails
    //   because completely unrelated mocks now get replayed.
    if (this.mockControl) {
      try {
        this.mockControl.$verifyAll();
        this.mockControl.$replayAll();
        this.mockControl.$verifyAll();
      } finally {
        this.mockControl.$resetAll();
        if (this.shouldMakeMockControl_) {
          // If we created the mockControl, we'll also tear it down.
          this.mockControl.$tearDown();
        }
      }
    }
    // Verifying the mockControl may throw, so if cleanup needs to
    // happen, add it further up in the function.
  }

  /**
   * Create a new {@see MockControl} accessible via
   * `env.mockControl` for each test. If your test has more than one
   * testing environment, don't call this on more than one of them.
   * @return {!Environment} For chaining.
   */
  withMockControl() {
    if (!this.shouldMakeMockControl_) {
      this.shouldMakeMockControl_ = true;
      this.mockControl = new MockControl();
    }
    return this;
  }

  /**
   * Create a {@see MockClock} for each test. The clock will be
   * installed (override i.e. setTimeout) by default. It can be accessed
   * using `env.mockClock`. If your test has more than one testing
   * environment, don't call this on more than one of them.
   * @return {!Environment} For chaining.
   */
  withMockClock() {
    if (!this.hasMockClock()) {
      this.mockClockOn = true;
      this.mockClock = new MockClock(true);
    }
    return this;
  }

  /**
   * @return {boolean}
   * @protected
   */
  hasMockClock() {
    return this.mockClockOn && !!this.mockClock && !this.mockClock.isDisposed();
  }

  /**
   * Creates a basic strict mock of a `toMock`. For more advanced mocking,
   * please use the MockControl directly.
   * @param {?Function|?Object} toMock
   * @return {?}
   */
  mock(toMock) {
    if (!this.shouldMakeMockControl_) {
      throw new Error(
          'MockControl not available on this environment. ' +
          'Call withMockControl if this environment is expected ' +
          'to contain a MockControl.');
    }
    const mock = this.mockControl.createStrictMock(toMock);
    // Mocks are not type-checkable. To reduce burden on tests that are
    // type checked, this is typed as "?" to turn off JSCompiler
    // checking.
    // TODO(user): Enable a type-checked mocking library.
    return /** @type {?} */ (mock);
  }

  /**
   * Creates a basic loose mock of a `toMock`. For more advanced mocking,
   * please use the MockControl directly.
   * @param {?Function|?Object} toMock
   * @param {boolean=} ignoreUnexpectedCalls Defaults to false.
   * @return {?}
   */
  looseMock(toMock, ignoreUnexpectedCalls = false) {
    if (!this.shouldMakeMockControl_) {
      throw new Error(
          'MockControl not available on this environment. ' +
          'Call withMockControl if this environment is expected ' +
          'to contain a MockControl.');
    }
    const mock =
        this.mockControl.createLooseMock(toMock, ignoreUnexpectedCalls);
    // Mocks are not type-checkable. To reduce burden on tests that are type
    // checked, this is typed as "?" to turn off JSCompiler checking.
    // TODO(user): Enable a type-checked mocking library.
    return /** @type {?} */ (mock);
  }
}

/**
 * @private {?EnvironmentTestCase}
 */
Environment.activeTestCase_ = null;

// TODO(johnlenz): make this package private when it moves out of labs.
/**
 * @return {?TestCase}
 * @nocollapse
 */
Environment.getTestCaseIfActive = function() {
  return Environment.activeTestCase_;
};

/** @private @const {!DebugConsole} */
Environment.console_ = new DebugConsole();

// Activate logging to the browser's console by default.
Environment.console_.setCapturing(true);

/**
 * An internal TestCase used to hook environments into the JsUnit test runner.
 * Environments cannot be used in conjunction with custom TestCases for JsUnit.
 * @private @final @constructor
 * @extends {TestCase}
 */
function EnvironmentTestCase() {
  EnvironmentTestCase.base(this, 'constructor', document.title);

  /** @private {!Array<!Environment>}> */
  this.environments_ = [];

  /** @private {!Object} */
  this.testobj_ = goog.global;  // default

  // Automatically install this TestCase when any environment is used in a test.
  TestCase.initializeTestRunner(this);
}
goog.inherits(EnvironmentTestCase, TestCase);

/**
 * Override setLifecycleObj to allow incoming test object to provide only
 * runTests and shouldRunTests. The other lifecycle methods are controlled by
 * this environment.
 * @param {!Object} obj
 * @override
 */
EnvironmentTestCase.prototype.setLifecycleObj = function(obj) {
  asserts.assert(
      this.testobj_ == goog.global,
      'A test method object has already been provided ' +
          'and only one is supported.');

  // Store the test object so we can call lifecyle methods when needed.
  this.testobj_ = obj;

  if (this.testobj_['runTests']) {
    this.runTests = this.testobj_['runTests'].bind(this.testobj_);
  }
  if (this.testobj_['shouldRunTests']) {
    this.shouldRunTests = this.testobj_['shouldRunTests'].bind(this.testobj_);
  }
};

/**
 * @override
 * @return {!TestCase.Test}
 */
EnvironmentTestCase.prototype.createTest = function(
    name, ref, scope, objChain) {
  return new EnvironmentTest(name, ref, scope, objChain);
};

/**
 * Adds an environment to the JsUnit test.
 * @param {!Environment} env
 * @private
 */
EnvironmentTestCase.prototype.registerEnvironment_ = function(env) {
  this.environments_.push(env);
};

/**
 * @override
 * @return {!IThenable<*>|undefined}
 */
EnvironmentTestCase.prototype.setUpPage = function() {
  const setUpPageFns = this.environments_.map(env => {
    return () => env.setUpPage();
  });

  // User defined setUpPage method.
  if (this.testobj_['setUpPage']) {
    setUpPageFns.push(() => this.testobj_['setUpPage']());
  }
  return this.callAndChainPromises_(setUpPageFns);
};

/**
 * @override
 * @return {!IThenable<*>|undefined}
 */
EnvironmentTestCase.prototype.setUp = function() {
  const setUpFns = [];
  // User defined configure method.
  if (this.testobj_['configureEnvironment']) {
    setUpFns.push(() => this.testobj_['configureEnvironment']());
  }
  const test = this.getCurrentTest();
  if (test instanceof EnvironmentTest) {
    setUpFns.push(...test.configureEnvironments);
  }

  this.environments_.forEach(env => {
    setUpFns.push(() => env.setUp());
  }, this);

  // User defined setUp method.
  if (this.testobj_['setUp']) {
    setUpFns.push(() => this.testobj_['setUp']());
  }
  return this.callAndChainPromises_(setUpFns);
};

/**
 * Calls a chain of methods and makes sure to properly chain them if any of the
 * methods returns a thenable.
 * @param {!Array<function()>} fns
 * @param {boolean=} ensureAllFnsCalled If true, this method calls each function
 *     even if one of them throws an Error or returns a rejected Promise. If
 *     there were any Errors thrown (or Promises rejected), the first Error will
 *     be rethrown after all of the functions are called.
 * @return {!IThenable<*>|undefined}
 * @private
 */
EnvironmentTestCase.prototype.callAndChainPromises_ = function(
    fns, ensureAllFnsCalled) {
  // Using await here (and making callAndChainPromises_ an async method)
  // causes many tests across google3 to start failing with errors like this:
  // "Timed out while waiting for a promise returned from setUp to resolve".

  const isThenable = (v) => Thenable.isImplementedBy(v) ||
      (typeof goog.global['Promise'] === 'function' &&
       v instanceof goog.global['Promise']);

  // Record the first error that occurs so that it can be rethrown in the case
  // where ensureAllFnsCalled is set.
  let firstError;
  const recordFirstError = (e) => {
    if (!firstError) {
      firstError = e instanceof Error ? e : new Error(e);
    }
  };

  // Call the fns, chaining results that are Promises.
  let lastFnResult;
  for (const fn of fns) {
    if (isThenable(lastFnResult)) {
      // The previous fn was async, so chain the next fn.
      const rejectedHandler = ensureAllFnsCalled ? (e) => {
        recordFirstError(e);
        return fn();
      } : undefined;
      lastFnResult = lastFnResult.then(() => fn(), rejectedHandler);
    } else {
      // The previous fn was not async, so simply call the next fn.
      try {
        lastFnResult = fn();
      } catch (e) {
        if (!ensureAllFnsCalled) {
          throw e;
        }
        recordFirstError(e);
      }
    }
  }

  // After all of the fns have been called, either throw the first error if
  // there was one, or otherwise return the result of the last fn.
  const resultFn = () => {
    if (firstError) {
      throw firstError;
    }
    return lastFnResult;
  };
  return isThenable(lastFnResult) ? lastFnResult.then(resultFn, resultFn) :
                                    resultFn();
};

/**
 * @override
 * @return {!IThenable<*>|undefined}
 */
EnvironmentTestCase.prototype.tearDown = function() {
  const tearDownFns = [];
  // User defined tearDown method.
  if (this.testobj_['tearDown']) {
    tearDownFns.push(() => this.testobj_['tearDown']());
  }

  // Execute the tearDown methods for the environment in the reverse order
  // in which they were registered to "unfold" the setUp.
  const reverseEnvironments = [...this.environments_].reverse();
  reverseEnvironments.forEach(env => {
    tearDownFns.push(() => env.tearDown());
  });
  // For tearDowns between tests make sure they run as much as possible to avoid
  // interference between tests.
  return this.callAndChainPromises_(
      tearDownFns, /* ensureAllFnsCalled= */ true);
};

/** @override */
EnvironmentTestCase.prototype.tearDownPage = function() {
  // User defined tearDownPage method.
  if (this.testobj_['tearDownPage']) {
    this.testobj_['tearDownPage']();
  }

  const reverseEnvironments = [...this.environments_].reverse();
  reverseEnvironments.forEach(env => {
    env.tearDownPage();
  });
};

/**
 * An internal Test used to hook environments into the JsUnit test runner.
 * @param {string} name The test name.
 * @param {function()} ref Reference to the test function or test object.
 * @param {?Object=} scope Optional scope that the test function should be
 *     called in.
 * @param {!Array<!Object>=} objChain A chain of objects used to populate setUps
 *     and tearDowns.
 * @private
 * @final
 * @constructor
 * @extends {TestCase.Test}
 */
function EnvironmentTest(name, ref, scope, objChain) {
  EnvironmentTest.base(this, 'constructor', name, ref, scope, objChain);

  /**
   * @type {!Array<function()>}
   */
  this.configureEnvironments =
      (objChain || [])
          .filter((obj) => typeof obj.configureEnvironment === 'function')
          .map(/**
                * @param  {{configureEnvironment: function()}} obj
                * @return {function()}
                */
               function(obj) {
                 return obj.configureEnvironment.bind(obj);
               });
}
goog.inherits(EnvironmentTest, TestCase.Test);

exports = Environment;
