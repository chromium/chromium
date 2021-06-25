// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled settings prefs.
 */

/** @polymerBehavior */
/* #export */ const CrPolicyPrefBehavior = {
  properties: {
    /**
     * Showing that an extension is controlling a pref is sometimes done with a
     * different UI (e.g. extension-controlled-indicator). In  those cases,
     * avoid showing an (extra) indicator here.
     * @public
     */
    noExtensionIndicator: Boolean,
  },

  /**
   * Is the |pref| controlled by something that prevents user control of the
   * preference.
   * @return {boolean} True if |this.pref| is controlled by an enforced policy.
   */
  isPrefEnforced() {
    return !!this.pref &&
        this.pref.enforcement === chrome.settingsPrivate.Enforcement.ENFORCED;
  },

  /**
   * @return {boolean} True if |this.pref| has a recommended or enforced policy.
   */
  hasPrefPolicyIndicator() {
    if (!this.pref) {
      return false;
    }
    if (this.noExtensionIndicator &&
        this.pref.controlledBy ===
            chrome.settingsPrivate.ControlledBy.EXTENSION) {
      return false;
    }
    return this.isPrefEnforced() ||
        this.pref.enforcement ===
        chrome.settingsPrivate.Enforcement.RECOMMENDED;
  },
};

/** @interface */
/* #export */ class CrPolicyPrefBehaviorInterface {
  /** @param {boolean} enabled */
  set noExtensionIndicator(enabled) {}

  /** @return {boolean} */
  isPrefEnforced() {}
}
