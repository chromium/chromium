// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration toggle.
 */
Polymer({
  is: 'network-config-toggle',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    NetworkConfigElementBehavior,
  ],

  properties: {
    label: String,

    subLabel: String,

    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      notify: true,
    },

    /**
     * Uses Settings styling when true (policy icon is left of the toggle)
     */
    policyOnLeft: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },
  },

  listeners: {
    'click': 'onHostTap_',
  },

  /** @override */
  focus() {
    this.$$('cr-toggle').focus();
  },

  /**
   * Handles non cr-toggle button clicks (cr-toggle handles its own click events
   * which don't bubble).
   * @param {!Event} e
   * @private
   */
  onHostTap_(e) {
    e.stopPropagation();
    if (this.getDisabled_(this.disabled, this.property)) {
      return;
    }
    this.checked = !this.checked;
    this.fire('change');
  },
});
