// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration input fields.
 */
import '../../../cr_elements/cr_input/cr_input.js';
import '../../../cr_elements/cr_shared_vars.css.js';
import './cr_policy_network_indicator_mojo.js';
import './network_shared_css.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyNetworkBehaviorMojo} from './cr_policy_network_behavior_mojo.js';
import {NetworkConfigElementBehavior} from './network_config_element_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'network-config-input',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    NetworkConfigElementBehavior,
  ],

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

  focus() {
    this.$$('cr-input').focus();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeypress_(event) {
    if (event.key !== 'Enter') {
      return;
    }
    event.stopPropagation();
    this.fire('enter');
  },
});
