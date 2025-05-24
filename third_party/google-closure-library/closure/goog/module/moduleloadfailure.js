/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview The possible reasons for a module load failure callback being
 * fired. Moved to a separate file to allow it to be used across packages.
 */

goog.module('goog.module.ModuleLoadFailure');
goog.module.declareLegacyNamespace();

class ModuleLoadFailure {
  /**
   * @param {!ModuleLoadFailure.Type} type
   * @param {number=} status Optional http error status associated with this
   *     failure. This should be `undefined` if there was no associated http
   *     error status (i.e. do not use values like -1).
   */
  constructor(type, status = undefined) {
    /** @const {!ModuleLoadFailure.Type} */
    this.type = type;
    /** @const {number|undefined} */
    this.status = status;
  }

  /**
   * @return {string}
   * @override
   */
  toString() {
    return `${this.getReadableError_()} (${
        this.status != undefined ? this.status : '?'})`;
  }

  /**
   * Gets a human readable error message for a failure type.
   * @return {string} The readable error message.
   * @private
   */
  getReadableError_() {
    switch (this.type) {
      case ModuleLoadFailure.Type.UNAUTHORIZED:
        return 'Unauthorized';
      case ModuleLoadFailure.Type.CONSECUTIVE_FAILURES:
        return 'Consecutive load failures';
      case ModuleLoadFailure.Type.TIMEOUT:
        return 'Timed out';
      case ModuleLoadFailure.Type.OLD_CODE_GONE:
        return 'Out of date module id';
      case ModuleLoadFailure.Type.INIT_ERROR:
        return 'Init error';
      default:
        return `Unknown failure type ${this.type}`;
    }
  }
}

/**
 * The possible reasons for a module load failure callback being fired.
 * @enum {number}
 */
const Type = {
  /** 401 Status. */
  UNAUTHORIZED: 0,

  /** Error status (not 401) returned multiple times. */
  CONSECUTIVE_FAILURES: 1,

  /** Request timeout. */
  TIMEOUT: 2,

  /** 410 status, old code gone. */
  OLD_CODE_GONE: 3,

  /** The onLoad callbacks failed. */
  INIT_ERROR: 4
};

exports = ModuleLoadFailure;
exports.Type = Type;
