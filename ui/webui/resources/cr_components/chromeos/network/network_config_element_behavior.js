// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for network config elements.
 */

/** @polymerBehavior */
const NetworkConfigElementBehavior = {
  properties: {
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Network managed property associated with the config element.
     * @type {?OncMojo.ManagedProperty}
     */
    property: {
      type: Object,
      value: null,
    },
  },

  /**
   * @param {boolean} disabled
   * @param {?OncMojo.ManagedProperty} property
   * @return {boolean} True if the element should be disabled.
   * @private
   */
  getDisabled_: function(disabled, property) {
    return disabled || (!!property && this.isNetworkPolicyEnforced(property));
  },
};
