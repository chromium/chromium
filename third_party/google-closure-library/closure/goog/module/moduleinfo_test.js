/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.module.ModuleInfoTest');
goog.setTestOnly();

const BaseModule = goog.require('goog.module.BaseModule');
const MockClock = goog.require('goog.testing.MockClock');
const ModuleInfo = goog.require('goog.module.ModuleInfo');
const testSuite = goog.require('goog.testing.testSuite');

let mockClock;

class TestModule extends BaseModule {
  constructor() {
    super();
  }
}

testSuite({
  setUp() {
    mockClock = new MockClock(true);
  },

  tearDown() {
    mockClock.uninstall();
  },

  /** Test initial state of module info. */
  testNotLoadedAtStart() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m = new ModuleInfo();
    assertFalse('Shouldn\'t be loaded', m.isLoaded());
  },

  /**
     Test loaded module info. @suppress {checkTypes} suppression added to
     enable type checking
   */
  testOnLoad() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m = new ModuleInfo();

    m.setModuleConstructor(TestModule);
    m.onLoad(/** @type {?} */ (goog.nullFunction));
    assertTrue(m.isLoaded());

    const module = m.getModule();
    assertNotNull(module);
    assertTrue(module instanceof TestModule);

    m.dispose();
    assertTrue(m.isDisposed());
    assertTrue(
        'Disposing of ModuleInfo should dispose of its module',
        module.isDisposed());
  },

  /**
     Test callbacks on module load. @suppress {checkTypes} suppression added to
     enable type checking
   */
  testCallbacks() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m = new ModuleInfo();
    m.setModuleConstructor(TestModule);
    let index = 0;
    let a = -1;
    let b = -1;
    let c = -1;
    let d = -1;

    const ca = m.registerCallback(() => {
      a = index++;
    });
    const cb = m.registerCallback(() => {
      b = index++;
    });
    const cc = m.registerCallback(() => {
      c = index++;
    });
    const cd = m.registerEarlyCallback(() => {
      d = index++;
    });
    cb.abort();
    m.onLoad(/** @type {?} */ (goog.nullFunction));

    assertTrue('callback A should have fired', a >= 0);
    assertFalse('callback B should have been aborted', b >= 0);
    assertTrue('callback C should have fired', c >= 0);
    assertTrue('early callback d should have fired', d >= 0);

    assertEquals('ordering of callbacks was wrong', 0, d);
    assertEquals('ordering of callbacks was wrong', 1, a);
    assertEquals('ordering of callbacks was wrong', 2, c);
  },

  testErrorsInCallbacks() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m = new ModuleInfo();
    m.setModuleConstructor(TestModule);
    m.registerCallback(() => {
      throw new Error('boom1');
    });
    m.registerCallback(() => {
      throw new Error('boom2');
    });
    /** @suppress {checkTypes} suppression added to enable type checking */
    const hadError = m.onLoad(goog.nullFunction);
    assertTrue(hadError);

    const e = assertThrows(() => {
      mockClock.tick();
    });

    assertEquals('boom1', e.message);
  },

  /**
     Tests the error callbacks. @suppress {checkTypes} suppression added to
     enable type checking
   */
  testErrbacks() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const m = new ModuleInfo();
    m.setModuleConstructor(TestModule);
    let index = 0;
    let a = -1;
    let b = -1;
    let c = -1;
    const d = -1;

    const ca = m.registerErrback(() => {
      a = index++;
    });
    const cb = m.registerErrback(() => {
      b = index++;
    });
    const cc = m.registerErrback(() => {
      c = index++;
    });
    m.onError(/** @type {?} */ ('foo'));

    assertTrue('callback A should have fired', a >= 0);
    assertTrue('callback B should have fired', b >= 0);
    assertTrue('callback C should have fired', c >= 0);

    assertEquals('ordering of callbacks was wrong', 0, a);
    assertEquals('ordering of callbacks was wrong', 1, b);
    assertEquals('ordering of callbacks was wrong', 2, c);
  },
});
