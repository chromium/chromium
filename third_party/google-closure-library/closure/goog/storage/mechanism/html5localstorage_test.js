/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.mechanism.HTML5LocalStorageTest');
goog.setTestOnly();

const HTML5LocalStorage = goog.require('goog.storage.mechanism.HTML5LocalStorage');
const iterableMechanismTester = goog.require('goog.storage.mechanism.iterableMechanismTester');
/** @suppress {extraRequire} */
const mechanismSeparationTester = goog.require('goog.storage.mechanism.mechanismSeparationTester');
/** @suppress {extraRequire} */
const mechanismSharingTester = goog.require('goog.storage.mechanism.mechanismSharingTester');
/** @suppress {extraRequire} */
const mechanismTestDefinition = goog.require('goog.storage.mechanism.mechanismTestDefinition');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');

testSuite({
  shouldRunTests() {
    // Disabled in Safari because Apple SafariDriver runs tests in Private
    // Browsing mode, and Safari does not permit writing to localStorage in
    // Private Browsing windows.
    return !product.SAFARI;
  },

  setUp() {
    const localStorage = new HTML5LocalStorage();
    if (localStorage.isAvailable()) {
      /** @suppress {const} suppression added to enable type checking */
      mechanism = localStorage;
      // There should be at least 2 MiB.
      /** @suppress {const} suppression added to enable type checking */
      minimumQuota = 2 * 1024 * 1024;
      /** @suppress {const} suppression added to enable type checking */
      mechanism_shared = new HTML5LocalStorage();
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  tearDown() {
    if (!!mechanism) {
      mechanism.clear();
      /** @suppress {const} suppression added to enable type checking */
      mechanism = null;
    }
    if (!!mechanism_shared) {
      mechanism_shared.clear();
      /** @suppress {const} suppression added to enable type checking */
      mechanism_shared = null;
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAvailability() {
    assertNotNull(mechanism);
    assertTrue(mechanism.isAvailable());
    assertNotNull(mechanism_shared);
    assertTrue(mechanism_shared.isAvailable());
  },

  testCount() {
    assertNotNull(mechanism);
    iterableMechanismTester.testCount(
        /** @type {!HTML5LocalStorage} */ (mechanism));
  },
  testIteratorBasics() {
    assertNotNull(mechanism);
    iterableMechanismTester.testIteratorBasics(
        /** @type {!HTML5LocalStorage} */ (mechanism));
  },
  testIteratorWithTwoValues() {
    assertNotNull(mechanism);
    iterableMechanismTester.testIteratorWithTwoValues(
        /** @type {!HTML5LocalStorage} */ (mechanism));
  },
  testClear() {
    assertNotNull(mechanism);
    iterableMechanismTester.testClear(
        /** @type {!HTML5LocalStorage} */ (mechanism));
  },
  testClearClear() {
    assertNotNull(mechanism);
    iterableMechanismTester.testClearClear(
        /** @type {!HTML5LocalStorage} */ (mechanism));
  },
});
