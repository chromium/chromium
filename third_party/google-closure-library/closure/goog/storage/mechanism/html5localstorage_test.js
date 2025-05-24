/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.mechanism.HTML5LocalStorageTest');
goog.setTestOnly();

const HTML5LocalStorage = goog.require('goog.storage.mechanism.HTML5LocalStorage');
const iterableMechanismTests = goog.require('goog.storage.mechanism.iterableMechanismTests');
const mechanismSharingTests = goog.require('goog.storage.mechanism.mechanismSharingTests');
const mechanismTests = goog.require('goog.storage.mechanism.mechanismTests');
const product = goog.require('goog.userAgent.product');
const testSuite = goog.require('goog.testing.testSuite');

let mechanism;
let minimumQuota;
let mechanismShared;

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
      mechanism = localStorage;
      // There should be at least 2 MiB.
      minimumQuota = 2 * 1024 * 1024;
      mechanismShared = new HTML5LocalStorage();
    }
  },

  tearDown() {
    if (!!mechanism) {
      mechanism.clear();
      mechanism = null;
    }
    if (!!mechanismShared) {
      mechanismShared.clear();
      mechanismShared = null;
    }
  },

  testAvailability() {
    assertNotNull(mechanism);
    assertTrue(mechanism.isAvailable());
    assertNotNull(mechanismShared);
    assertTrue(mechanismShared.isAvailable());
  },

  ...mechanismTests.register({
    getMechanism: function() {
      return mechanism;
    },
    getMinimumQuota: function() {
      return minimumQuota;
    },
  }),

  ...iterableMechanismTests.register({
    getMechanism: function() {
      return mechanism;
    },
  }),

  ...mechanismSharingTests.register({
    getMechanism: function() {
      return mechanism;
    },
    getMechanismShared: function() {
      return mechanismShared;
    },
  }),
});
