/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.testing.singletonTest');
goog.setTestOnly();

const asserts = goog.require('goog.testing.asserts');
const singleton = goog.require('goog.testing.singleton');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  testGetInstance() {
    function SingletonClass() {}
    goog.addSingletonGetter(SingletonClass);

    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const s1 = SingletonClass.getInstance();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const s2 = SingletonClass.getInstance();
    assertEquals('second getInstance call returns the same instance', s1, s2);

    singleton.resetAll();
    /**
     * @suppress {missingProperties} suppression added to enable type checking
     */
    const s3 = SingletonClass.getInstance();
    assertNotEquals('getInstance returns a new instance after reset', s1, s3);
  },

  testReset() {
    class Singleton {}
    goog.addSingletonGetter(Singleton);

    class OtherSingleton {}
    goog.addSingletonGetter(OtherSingleton);

    const instance1 = Singleton.getInstance();
    const instance2 = OtherSingleton.getInstance();

    singleton.reset(Singleton);
    assertNotEquals(instance1, Singleton.getInstance());
    assertEquals(instance2, OtherSingleton.getInstance());
  },
});
