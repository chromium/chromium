/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Defines errors to be thrown by the storage.
 */

goog.provide('goog.storage.ErrorCode');


/**
 * Errors thrown by the storage.
 * @enum {string}
 */
goog.storage.ErrorCode = {
  INVALID_VALUE: 'Storage: Invalid value was encountered',
  DECRYPTION_ERROR: 'Storage: The value could not be decrypted'
};
