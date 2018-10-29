// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration input fields.
 */
Polymer({
  is: 'network-config-input',

  behaviors: [CrPolicyNetworkBehavior, NetworkConfigElementBehavior],

  properties: {
    label: String,

    hidden: {
      type: Boolean,
      reflectToAttribute: true,
    },

    readonly: {
      type: Boolean,
      reflectToAttribute: true,
    },

    value: {
      type: String,
      notify: true,
    },
  },

  focus: function() {
    this.$$('cr-input').focus();
  },
});
