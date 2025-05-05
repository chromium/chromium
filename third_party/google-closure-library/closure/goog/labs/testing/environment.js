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
const {EnvironmentBase} = goog.require('goog.labs.testing.EnvironmentBase');

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
class Environment extends EnvironmentBase {
  constructor() {
    super();
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

    /** @const {!DebugConsole} */
    this.console = Environment.console_;

    /** @const {!PropertyReplacer} */
    this.replacer = new PropertyReplacer();
  }

  /**
   * Runs immediately before the setUpPage phase of JsUnit tests.
   * @return {!IThenable<*>|undefined} An optional Promise which must be
   *     resolved before the test is executed.
   * @override
   */
  setUpPage() {
    if (this.hasMockClock()) {
      this.mockClock.install();
    }
  }

  /** @override Runs immediately after the tearDownPage phase of JsUnit tests */
  tearDownPage() {
    // If we created the mockClock, we'll also dispose it.
    if (this.hasMockClock()) {
      this.mockClock.uninstall();
    }
  }


  /**
   * Runs immediately after the tearDown phase of JsUnit tests.
   * @return {!IThenable<*>|undefined} An optional Promise which must be
   *     resolved before the next test case is executed.
   * @override
   */
  tearDown() {
    // Make sure promises and other stuff that may still be scheduled,
    // get a
    // chance to run (and throw errors).
    if (this.hasMockClock()) {
      if (this.mockClock.isSynchronous()) {
        for (let i = 0; i < 100; i++) {
          this.mockClock.tick(1000);
        }
      }
      this.mockClock.reset();
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
   * @param {{install: (boolean|undefined), async: (boolean|undefined)}=}
   *     options Options about the mockClock.
   * @return {!Environment} For chaining.
   */
  withMockClock({install = true, async = false} = {}) {
    if (!this.hasMockClock() || this.mockClock.isSynchronous() === async) {
      this.mockClock =
          async ? MockClock.createAsyncMockClock() : new MockClock();
      if (install) {
        this.mockClock.install();
      }
    }
    return this;
  }

  /**
   * @return {boolean}
   * @protected
   */
  hasMockClock() {
    return !!this.mockClock && !this.mockClock.isDisposed();
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

/** @private @const {!DebugConsole} */
Environment.console_ = new DebugConsole();

// Activate logging to the browser's console by default.
Environment.console_.setCapturing(true);

exports = Environment;
