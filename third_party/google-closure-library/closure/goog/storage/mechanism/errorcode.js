/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Defines error codes to be thrown by storage mechanisms.
 */

goog.provide('goog.storage.mechanism.ErrorCode');


/**
 * Errors thrown by storage mechanisms.
 * @enum {string}
 */
goog.storage.mechanism.ErrorCode = {
  INVALID_VALUE: 'Storage mechanism: Invalid value was encountered',
  QUOTA_EXCEEDED: 'Storage mechanism: Quota exceeded',
  STORAGE_DISABLED: 'Storage mechanism: Storage disabled'
};
