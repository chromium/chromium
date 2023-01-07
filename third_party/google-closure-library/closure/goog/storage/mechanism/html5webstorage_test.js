/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.storage.mechanism.HTML5WebStorageTest');
goog.setTestOnly('goog.storage.mechanism.HTML5WebStorageTest');

const ErrorCode = goog.require('goog.storage.mechanism.ErrorCode');
const HTML5WebStorage = goog.require('goog.storage.mechanism.HTML5WebStorage');
const testSuite = goog.require('goog.testing.testSuite');

/**
 * A minimal WebStorage implementation that throws exceptions for disabled
 * storage. Since we cannot have unit tests running in Safari private mode to
 * test this, we need to mock an exception throwing when trying to set a value.
 * @unrestricted
 */
class MockThrowableStorage {
  /**
   * @param {boolean=} opt_isStorageDisabled If true, throws exceptions
   *     emulating Private browsing mode.  If false, storage quota will be
   *     marked as exceeded.
   */
  constructor(opt_isStorageDisabled) {
    this.isStorageDisabled_ = !!opt_isStorageDisabled;
    this.length = opt_isStorageDisabled ? 0 : 1;
  }

  /**
   * @override
   * @suppress {checkTypes} suppression added to enable type checking
   */
  setItem(key, value) {
    if (this.isStorageDisabled_) {
      throw ErrorCode.STORAGE_DISABLED;
    } else {
      throw ErrorCode.QUOTA_EXCEEDED;
    }
  }

  /**
   * @override
   * @suppress {checkTypes} suppression added to enable type checking
   */
  removeItem(key) {}

  /**
   * A very simple, dummy implementation of key(), merely to verify that calls
   * to HTML5WebStorage#key are proxied through.
   * @param {number} index A key index.
   * @return {string} The key associated with that index.
   */
  key(index) {
    return 'dummyKey';
  }
}



/**
 * Provides an HTML5WebStorage wrapper for MockThrowableStorage.
 * @unrestricted
 */
class HTML5MockStorage extends HTML5WebStorage {
  /** @suppress {checkTypes} suppression added to enable type checking */
  constructor(opt_isStorageDisabled) {
    super(new MockThrowableStorage(opt_isStorageDisabled));
  }
}



testSuite({
  testIsNotAvailableWhenQuotaExceeded() {
    const storage = new HTML5MockStorage(false);
    assertFalse(storage.isAvailable());
  },

  testIsNotAvailableWhenStorageDisabled() {
    const storage = new HTML5MockStorage(true);
    assertFalse(storage.isAvailable());
  },

  testSetThrowsExceptionWhenQuotaExceeded() {
    const storage = new HTML5MockStorage(false);
    let isQuotaExceeded = false;
    try {
      storage.set('foobar', '1');
    } catch (e) {
      isQuotaExceeded = e == ErrorCode.QUOTA_EXCEEDED;
    }
    assertTrue(isQuotaExceeded);
  },

  testSetThrowsExceptionWhenStorageDisabled() {
    const storage = new HTML5MockStorage(true);
    let isStorageDisabled = false;
    try {
      storage.set('foobar', '1');
    } catch (e) {
      isStorageDisabled = e == ErrorCode.STORAGE_DISABLED;
    }
    assertTrue(isStorageDisabled);
  },

  testKeyIterationWithKeyMethod() {
    const storage = new HTML5MockStorage(true);
    assertEquals('dummyKey', storage.key(1));
  },

  // Common functionality testing is done per-implementation.
});
