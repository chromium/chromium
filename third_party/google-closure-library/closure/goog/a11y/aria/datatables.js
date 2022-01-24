/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */



/**
 * @fileoverview The file contains data tables generated from the ARIA
 * standard schema http://www.w3.org/TR/wai-aria/.
 *
 * This is auto-generated code. Do not manually edit!
 */

goog.provide('goog.a11y.aria.datatables');

goog.require('goog.a11y.aria.State');
goog.require('goog.object');


/**
 * A map that contains mapping between an ARIA state and the default value
 * for it. Note that not all ARIA states have default values.
 *
 * @private {?Object<!goog.a11y.aria.State|string, string|boolean|number>}
 */
goog.a11y.aria.DefaultStateValueMap_;


/**
 * A method that creates a map that contains mapping between an ARIA state and
 * the default value for it. Note that not all ARIA states have default values.
 *
 * @return {!Object<!goog.a11y.aria.State|string, string|boolean|number>}
 *      The names for each of the notification methods.
 */
goog.a11y.aria.datatables.getDefaultValuesMap = function() {
  'use strict';
  if (!goog.a11y.aria.DefaultStateValueMap_) {
    goog.a11y.aria.DefaultStateValueMap_ = goog.object.create(
        goog.a11y.aria.State.ATOMIC, false, goog.a11y.aria.State.AUTOCOMPLETE,
        'none', goog.a11y.aria.State.DROPEFFECT, 'none',
        goog.a11y.aria.State.HASPOPUP, false, goog.a11y.aria.State.LIVE, 'off',
        goog.a11y.aria.State.MULTILINE, false,
        goog.a11y.aria.State.MULTISELECTABLE, false,
        goog.a11y.aria.State.ORIENTATION, 'vertical',
        goog.a11y.aria.State.READONLY, false, goog.a11y.aria.State.RELEVANT,
        'additions text', goog.a11y.aria.State.REQUIRED, false,
        goog.a11y.aria.State.SORT, 'none', goog.a11y.aria.State.BUSY, false,
        goog.a11y.aria.State.DISABLED, false, goog.a11y.aria.State.HIDDEN,
        false, goog.a11y.aria.State.INVALID, 'false');
  }

  return goog.a11y.aria.DefaultStateValueMap_;
};
