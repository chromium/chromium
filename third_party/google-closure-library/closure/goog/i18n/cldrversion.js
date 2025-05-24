/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @fileoverview This module houses utlities related to being able to change
 * what version of the CLDR dataset is used by Closure Library i18n packages.
 * The version of CLDR data to use is determined once during initial JS
 * evaluation and cached (using a goog.define). To ensure consistency of all
 * I18n operations throughout the lifetime of an application this module does
 * not support dynamically swapping the version of CLDR in use at runtime.
 */
goog.module('goog.i18n.cldrversion');
goog.module.declareLegacyNamespace();

/** @define {boolean} */
exports.USE_CLDR_NEXT = goog.define(
    'goog.i18n.USE_CLDR_NEXT_FOR_TESTING_USE_ONLY_PLEASE_DO_NOT_USE_IN_PRODUCTION',
    false);
