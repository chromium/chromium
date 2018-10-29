// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for network configuration selection menus.
 */
Polymer({
  is: 'network-config-select',

  behaviors: [
    I18nBehavior,
    CrPolicyNetworkBehavior,
    NetworkConfigElementBehavior,
  ],

  properties: {
    label: String,

    /** Set to true if |items| is a list of certificates. */
    certList: Boolean,

    /**
     * Array of item values to select from.
     * @type {!Array<string>}
     */
    items: Array,

    /** Prefix used to look up ONC property names. */
    oncPrefix: {
      type: String,
      value: '',
    },

    /** Select item value */
    value: {
      type: String,
      notify: true,
    },
  },

  observers: ['updateSelected_(items, value)'],

  focus: function() {
    this.$$('select').focus();
  },

  /**
   * Ensure that the <select> value is updated when |items| or |value| changes.
   * @private
   */
  updateSelected_: function() {
    // Wait for the dom-repeat to populate the <option> entries.
    this.async(function() {
      var select = this.$$('select');
      if (select.value != this.value)
        select.value = this.value;
    });
  },

  /**
   * @param {string|!chrome.networkingPrivate.Certificate} item
   * @param {string} prefix
   * @return {string} The text to display for the onc value.
   * @private
   */
  getItemLabel_: function(item, prefix) {
    if (this.certList) {
      return this.getCertificateName_(
          /** @type {chrome.networkingPrivate.Certificate}*/ (item));
    }
    var key = /** @type {string} */ (item);
    var oncKey = 'Onc' + prefix.replace(/\./g, '-') + '_' + key;
    if (this.i18nExists(oncKey))
      return this.i18n(oncKey);
    assertNotReached('ONC Key not found: ' + oncKey);
    return key;
  },

  /**
   * @param {string|!chrome.networkingPrivate.Certificate} item
   * @return {string}
   * @private
   */
  getItemValue_: function(item) {
    if (this.certList)
      return /** @type {chrome.networkingPrivate.Certificate}*/ (item).hash;
    return /** @type {string} */ (item);
  },

  /**
   * @param {string|!chrome.networkingPrivate.Certificate} item
   * @return {boolean}
   * @private
   */
  getItemEnabled_: function(item) {
    if (this.certList) {
      var cert = /** @type {chrome.networkingPrivate.Certificate}*/ (item);
      return !!cert.hash;
    }
    return true;
  },

  /**
   * @param {!chrome.networkingPrivate.Certificate} certificate
   * @return {string}
   * @private
   */
  getCertificateName_: function(certificate) {
    if (certificate.hardwareBacked) {
      return this.i18n(
          'networkCertificateNameHardwareBacked', certificate.issuedBy,
          certificate.issuedTo);
    }
    if (certificate.issuedTo) {
      return this.i18n(
          'networkCertificateName', certificate.issuedBy, certificate.issuedTo);
    }
    return certificate.issuedBy;
  },
});
