// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'start-setup-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'accept',
    },

    /** Overridden from UiPageContainerBehavior. */
    cancelButtonTextId: {
      type: String,
      computed: 'getCancelButtonTextId_(delegate)',
    },

    /** Overridden from UiPageContainerBehavior. */
    headerId: {
      type: String,
      value: 'startSetupPageHeader',
    },

    /** Overridden from UiPageContainerBehavior. */
    messageId: {
      type: String,
      value: 'startSetupPageMessage',
    },

    /**
     * Array of objects representing all potential MultiDevice hosts.
     *
     * @type {!Array<!chromeos.deviceSync.mojom.RemoteDevice>}
     */
    devices: {
      type: Array,
      value: () => [],
      observer: 'devicesChanged_',
    },

    /**
     * Unique identifier for the currently selected host device.
     *
     * Undefined if the no list of potential hosts has been received from mojo
     * service.
     *
     * @type {string|undefined}
     */
    selectedDeviceId: {
      type: String,
      notify: true,
    },

    /**
     * Delegate object which performs differently in OOBE vs. non-OOBE mode.
     * @type {!multidevice_setup.MultiDeviceSetupDelegate}
     */
    delegate: Object,
  },

  behaviors: [
    UiPageContainerBehavior,
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  /** @override */
  attached: function() {
    this.addWebUIListener(
        'multidevice_setup.initializeSetupFlow',
        this.initializeSetupFlow_.bind(this));
  },

  /** @private */
  initializeSetupFlow_: function() {
    // The "Learn More" links are inside a grdp string, so we cannot actually
    // add an onclick handler directly to the html. Instead, grab the two and
    // manaully add onclick handlers.
    let helpArticleLinks = [
      this.$$('#multidevice-summary-message a'),
      this.$$('#awm-summary-message a')
    ];
    for (let i = 0; i < helpArticleLinks.length; i++) {
      helpArticleLinks[i].onclick = this.fire.bind(
          this, 'open-learn-more-webview-requested', helpArticleLinks[i].href);
    }
  },

  /**
   * @param {!multidevice_setup.MultiDeviceSetupDelegate} delegate
   * @return {string} The cancel button text ID, dependent on OOBE vs. non-OOBE.
   * @private
   */
  getCancelButtonTextId_: function(delegate) {
    return this.delegate.getStartSetupCancelButtonTextId();
  },

  /**
   * @param {!Array<!chromeos.deviceSync.mojom.RemoteDevice>} devices
   * @return {string} Label for devices selection content.
   * @private
   */
  getDeviceSelectionHeader_(devices) {
    switch (devices.length) {
      case 0:
        return '';
      case 1:
        return this.i18n('startSetupPageSingleDeviceHeader');
      default:
        return this.i18n('startSetupPageMultipleDeviceHeader');
    }
  },

  /**
   * @param {!Array<!chromeos.deviceSync.mojom.RemoteDevice>} devices
   * @return {boolean} True if there are more than one potential host devices.
   * @private
   */
  doesDeviceListHaveMultipleElements_: function(devices) {
    return devices.length > 1;
  },

  /**
   * @param {!Array<!chromeos.deviceSync.mojom.RemoteDevice>} devices
   * @return {boolean} True if there is exactly one potential host device.
   * @private
   */
  doesDeviceListHaveOneElement_: function(devices) {
    return devices.length == 1;
  },

  /**
   * @param {!Array<!chromeos.deviceSync.mojom.RemoteDevice>} devices
   * @return {string} Name of the first device in device list if there are any.
   *     Returns an empty string otherwise.
   * @private
   */
  getFirstDeviceNameInList_: function(devices) {
    return devices[0] ? this.devices[0].deviceName : '';
  },

  /** @private */
  devicesChanged_: function() {
    if (this.devices.length > 0)
      this.selectedDeviceId = this.devices[0].deviceId;
  },

  /** @private */
  onDeviceDropdownSelectionChanged_: function() {
    this.selectedDeviceId = this.$.deviceDropdown.value;
  },
});
