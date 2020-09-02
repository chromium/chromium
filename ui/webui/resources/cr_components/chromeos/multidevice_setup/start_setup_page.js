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

    /**
     * Array of objects representing all potential MultiDevice hosts.
     *
     * @type {!Array<!chromeos.multideviceSetup.mojom.HostDevice>}
     */
    devices: {
      type: Array,
      value: () => [],
      observer: 'devicesChanged_',
    },

    /**
     * Unique identifier for the currently selected host device. This uses the
     * device's Instance ID if it is available; otherwise, the device's legacy
     * device ID is used.
     * TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, only
     * use Instance ID since all devices are guaranteed to have one.
     *
     * Undefined if the no list of potential hosts has been received from mojo
     * service.
     *
     * @type {string|undefined}
     */
    selectedInstanceIdOrLegacyDeviceId: {
      type: String,
      notify: true,
    },

    /**
     * Delegate object which performs differently in OOBE vs. non-OOBE mode.
     * @type {!multidevice_setup.MultiDeviceSetupDelegate}
     */
    delegate: Object,

    /** @private */
    phoneHubEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('phoneHubEnabled') &&
            loadTimeData.getBoolean('phoneHubEnabled');
      },
    },

    /** @private */
    wifiSyncEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('wifiSyncEnabled') &&
            loadTimeData.getBoolean('wifiSyncEnabled');
      },
    },
  },

  behaviors: [
    UiPageContainerBehavior,
    WebUIListenerBehavior,
  ],

  /** @override */
  attached() {
    this.addWebUIListener(
        'multidevice_setup.initializeSetupFlow',
        this.initializeSetupFlow_.bind(this));
  },

  /** @private */
  initializeSetupFlow_() {
    // The "Learn More" links are inside a grdp string, so we cannot actually
    // add an onclick handler directly to the html. Instead, grab the two and
    // manaully add onclick handlers.
    const helpArticleLinks = [
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
  getCancelButtonTextId_(delegate) {
    return this.delegate.getStartSetupCancelButtonTextId();
  },

  /**
   * @param {!Array<!chromeos.multideviceSetup.mojom.HostDevice>} devices
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
   * @param {!Array<!chromeos.multideviceSetup.mojom.HostDevice>} devices
   * @return {boolean} True if there are more than one potential host devices.
   * @private
   */
  doesDeviceListHaveMultipleElements_(devices) {
    return devices.length > 1;
  },

  /**
   * @param {!Array<!chromeos.multideviceSetup.mojom.HostDevice>} devices
   * @return {boolean} True if there is exactly one potential host device.
   * @private
   */
  doesDeviceListHaveOneElement_(devices) {
    return devices.length === 1;
  },

  /**
   * @param {!Array<!chromeos.multideviceSetup.mojom.HostDevice>} devices
   * @return {string} Name of the first device in device list if there are any.
   *     Returns an empty string otherwise.
   * @private
   */
  getFirstDeviceNameInList_(devices) {
    return devices[0] ? this.devices[0].remoteDevice.deviceName : '';
  },

  /**
   * @param {!chromeos.deviceSync.mojom.ConnectivityStatus} connectivityStatus
   * @return {string} The classes to bind to the device name option.
   * @private
   */
  getDeviceOptionClass_(connectivityStatus) {
    return connectivityStatus ===
            chromeos.deviceSync.mojom.ConnectivityStatus.kOffline ?
        'offline-device-name' :
        '';
  },

  /**
   * @param {!chromeos.multideviceSetup.mojom.HostDevice} device
   * @return {string} Name of the device, with connectivity status information.
   * @private
   */
  getDeviceNameWithConnectivityStatus_(device) {
    return device.connectivityStatus ===
            chromeos.deviceSync.mojom.ConnectivityStatus.kOffline ?
        this.i18n(
            'startSetupPageOfflineDeviceOption',
            device.remoteDevice.deviceName) :
        device.remoteDevice.deviceName;
  },

  /**
   * @param {!chromeos.multideviceSetup.mojom.HostDevice} device
   * @return {string} Returns a unique identifier for the input device, using
   *     the device's Instance ID if it is available; otherwise, the device's
   *     legacy device ID is used.
   *     TODO(https://crbug.com/1019206): When v1 DeviceSync is turned off, only
   *     use Instance ID since all devices are guaranteed to have one.
   * @private
   */
  getInstanceIdOrLegacyDeviceId_(device) {
    if (device.remoteDevice.instanceId) {
      return device.remoteDevice.instanceId;
    }

    return device.remoteDevice.deviceId;
  },

  /** @private */
  devicesChanged_() {
    if (this.devices.length > 0) {
      this.selectedInstanceIdOrLegacyDeviceId =
          this.getInstanceIdOrLegacyDeviceId_(this.devices[0]);
    }
  },

  /** @private */
  onDeviceDropdownSelectionChanged_() {
    this.selectedInstanceIdOrLegacyDeviceId = this.$.deviceDropdown.value;
  },

  /**
   * Wrapper for i18nAdvanced for binding to location updates in OOBE.
   * @param {string} locale The language code (e.g. en, es) for the current
   *     display language for CrOS. As with I18nBehavior.i18nDynamic(), the
   *     parameter is not used directly but is provided to allow HTML binding
   *     without passing an unexpected argument to I18nBehavior.i18nAdvanced().
   * @param {string} textId The loadTimeData ID of the string to be translated.
   * @private
   */
  i18nAdvancedDynamic_(locale, textId) {
    return this.i18nAdvanced(textId);
  },
});
