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

    checked: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      notify: true,
    },
  },
});
