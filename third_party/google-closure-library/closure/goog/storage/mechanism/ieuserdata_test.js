/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.mechanism.IEUserDataTest');
goog.setTestOnly();

const IEUserData = goog.require('goog.storage.mechanism.IEUserData');
/** @suppress {extraRequire} */
const mechanismSeparationTester = goog.require('goog.storage.mechanism.mechanismSeparationTester');
/** @suppress {extraRequire} */
const mechanismSharingTester = goog.require('goog.storage.mechanism.mechanismSharingTester');
/** @suppress {extraRequire} */
const mechanismTestDefinition = goog.require('goog.storage.mechanism.mechanismTestDefinition');
const testSuite = goog.require('goog.testing.testSuite');
const userAgent = goog.require('goog.userAgent');

testSuite({
  setUp() {
    const ieUserData = new IEUserData('test');
    if (ieUserData.isAvailable()) {
      /** @suppress {const} suppression added to enable type checking */
      mechanism = ieUserData;
      // There should be at least 32 KiB.
      /** @suppress {const} suppression added to enable type checking */
      minimumQuota = 32 * 1024;
      /** @suppress {const} suppression added to enable type checking */
      mechanism_shared = new IEUserData('test');
      /** @suppress {const} suppression added to enable type checking */
      mechanism_separate = new IEUserData('test2');
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
    if (!!mechanism_separate) {
      mechanism_separate.clear();
      /** @suppress {const} suppression added to enable type checking */
      mechanism_separate = null;
    }
  },

  /**
     @suppress {strictMissingProperties} suppression added to enable type
     checking
   */
  testAvailability() {
    if (userAgent.IE && !userAgent.isDocumentModeOrHigher(9)) {
      assertNotNull(mechanism);
      assertTrue(mechanism.isAvailable());
      assertNotNull(mechanism_shared);
      assertTrue(mechanism_shared.isAvailable());
      assertNotNull(mechanism_separate);
      assertTrue(mechanism_separate.isAvailable());
    }
  },

  testEncoding() {
    /** @suppress {visibility} suppression added to enable type checking */
    function assertEncodingPair(cleartext, encoded) {
      assertEquals(encoded, IEUserData.encodeKey_(cleartext));
      assertEquals(cleartext, IEUserData.decodeKey_(encoded));
    }
    assertEncodingPair('simple', '_simple');
    assertEncodingPair(
        'aa.bb%cc!\0$\u4e00.', '_aa.2Ebb.25cc.21.00.24.E4.B8.80.2E');
  },
});
