/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Policy to convert strings to Trusted Types. See
 * https://github.com/WICG/trusted-types for details.
 */

goog.provide('goog.html.trustedtypes');


/**
 * Cached result of goog.createTrustedTypesPolicy.
 * @type {?TrustedTypePolicy|undefined}
 * @private
 */
goog.html.trustedtypes.cachedPolicy_;


/**
 * Creates a (singleton) Trusted Type Policy for Safe HTML Types.
 * @return {?TrustedTypePolicy}
 * @package
 */
goog.html.trustedtypes.getPolicyPrivateDoNotAccessOrElse = function() {
  'use strict';
  if (!goog.TRUSTED_TYPES_POLICY_NAME) {
    // Binary not configured for Trusted Types.
    return null;
  }

  if (goog.html.trustedtypes.cachedPolicy_ === undefined) {
    goog.html.trustedtypes.cachedPolicy_ =
        goog.createTrustedTypesPolicy(goog.TRUSTED_TYPES_POLICY_NAME + '#html');
  }

  return goog.html.trustedtypes.cachedPolicy_;
};
