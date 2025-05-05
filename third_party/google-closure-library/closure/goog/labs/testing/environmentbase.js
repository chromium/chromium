/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.labs.testing.EnvironmentBase');

const TestCase = goog.require('goog.testing.TestCase');
const Thenable = goog.require('goog.Thenable');
const asserts = goog.require('goog.asserts');


/**
 * JsUnit environments allow developers to customize the existing testing
 * lifecycle by hitching additional setUp and tearDown behaviors to tests.
 *
 * Environments will run their setUp steps in the order in which they
 * are instantiated and registered. During tearDown, the environments will
 * unwind the setUp and execute in reverse order.
 *
 * This base class has no dependencies on mocking or goog.testing.asserts.
 */
class EnvironmentBase {
  constructor() {
    // Use the same EnvironmentTestCase instance across all EnvironmentBase
    // objects.
    if (!EnvironmentBase.activeTestCase_) {
      const testcase = new EnvironmentTestCase();

      EnvironmentBase.activeTestCase_ = testcase;
    }
    EnvironmentBase.activeTestCase_.registerEnvironment_(this);
  }

  /**
   * Runs immediately before the setUpPage phase of JsUnit tests.
   * @return {!IThenable<*>|undefined} An optional Promise which must be
   *     resolved before the test is executed.
   */
  setUpPage() {}

  /** Runs immediately after the tearDownPage phase of JsUnit tests. */
  tearDownPage() {}

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
  tearDown() {}
}

/**
 * @private {?EnvironmentTestCase}
 */
EnvironmentBase.activeTestCase_ = null;

// TODO(johnlenz): make this package private when it moves out of labs.
/**
 * @return {?TestCase}
 * @nocollapse
 */
EnvironmentBase.getTestCaseIfActive = function() {
  return EnvironmentBase.activeTestCase_;
};

/**
 * An internal TestCase used to hook environments into the JsUnit test runner.
 * Environments cannot be used in conjunction with custom TestCases for JsUnit.
 * @private @final @constructor
 * @extends {TestCase}
 */
function EnvironmentTestCase() {
  EnvironmentTestCase.base(this, 'constructor', document.title);

  /** @private @const {!Array<!EnvironmentBase>}> */
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
 * @param {!EnvironmentBase} env
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

exports = {EnvironmentBase};
