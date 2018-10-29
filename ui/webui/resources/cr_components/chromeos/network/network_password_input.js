// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network password input fields.
 */
Polymer({
  is: 'network-password-input',

  behaviors: [
    I18nBehavior,
    CrPolicyNetworkBehavior,
    NetworkConfigElementBehavior,
  ],

  properties: {
    label: {
      type: String,
      reflectToAttribute: true,
    },

    value: {
      type: String,
      notify: true,
    },

    showPassword: {
      type: Boolean,
      value: false,
    },
  },

  focus: function() {
    this.$$('cr-input').focus();
  },

  /**
   * @return {string}
   * @private
   */
  getInputType_: function() {
    return this.showPassword ? 'text' : 'password';
  },

  /**
   * @return {string}
   * @private
   */
  getIconClass_: function() {
    return this.showPassword ? 'icon-visibility-off' : 'icon-visibility';
  },

  /**
   * @return {string}
   * @private
   */
  getShowPasswordTitle_: function() {
    return this.showPassword ? this.i18n('hidePassword') :
                               this.i18n('showPassword');
  },

  /**
   * @param {!Event} event
   * @private
   */
  onShowPasswordTap_: function(event) {
    this.showPassword = !this.showPassword;
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onInputKeypress_: function(event) {
    if (event.target.id != 'input' || event.key != 'Enter')
      return;
    event.stopPropagation();
    this.fire('enter');
  },
});
