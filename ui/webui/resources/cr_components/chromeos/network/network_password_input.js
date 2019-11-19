// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network password input fields.
 */

// Used to indicate a saved but unknown credential value. Will appear as *'s in
// the credential (passphrase, password, etc.) field by default.
// See |kFakeCredential| in chromeos/network/policy_util.h.
/** @type {string} */ const FAKE_CREDENTIAL = 'FAKE_CREDENTIAL_VPaJDV9x';

Polymer({
  is: 'network-password-input',

  behaviors: [
    I18nBehavior,
    CrPolicyNetworkBehaviorMojo,
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

    showPolicyIndicator_: {
      type: Boolean,
      value: false,
      computed: 'getDisabled_(disabled, property)',
    },
  },

  /** @private */
  focus: function() {
    this.$$('cr-input').focus();

    // If the input has any contents, the should be selected when focus is
    // applied.
    this.$$('cr-input').select();
  },

  /**
   * @return {string}
   * @private
   */
  getInputType_: function() {
    return this.showPassword ? 'text' : 'password';
  },

  /**
   * @return {boolean}
   * @private
   */
  isShowingPlaceholder_: function() {
    return this.value == FAKE_CREDENTIAL;
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
    if (this.isShowingPlaceholder_()) {
      // Never show the actual placeholder, clear the field instead.
      this.value = '';
      this.focus();
    }

    this.showPassword = !this.showPassword;
    event.stopPropagation();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeypress_: function(event) {
    if (event.target.id == 'input' && event.key == 'Enter') {
      event.stopPropagation();
      this.fire('enter');
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onKeydown_: function(event) {
    if (!this.isShowingPlaceholder_()) {
      return;
    }

    if (event.key.indexOf('Arrow') < 0 && event.key != 'Home' &&
        event.key != 'End') {
      return;
    }

    // Prevent cursor navigation keys from working when the placeholder password
    // is displayed. This prevents using the arrows or home/end keys to
    // remove or change the selection.
    event.preventDefault();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onMousedown_: function(event) {
    if (!this.isShowingPlaceholder_()) {
      return;
    }

    if (document.activeElement != event.target) {
      // Focus the field and select the placeholder text if not already focused.
      this.focus();
    }

    // Prevent using the mouse or touchscreen to move the cursor or change the
    // selection when the placeholder password is displayed.  This prevents
    // the user from modifying the placeholder, only allows it to be left alone
    // or completely removed.
    event.preventDefault();
  },

});
