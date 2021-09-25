// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Root UI element for Bluetooth pairing dialog.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './bluetooth_pairing_device_selection_page.js';
import './bluetooth_pairing_request_code_page.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from '../../../js/assert.m.js';
import {getBluetoothConfig} from './cros_bluetooth_config.js';

/** @enum {string} */
const BluetoothPairingSubpageId = {
  // TODO(crbug.com/1010321): Add missing bluetooth pairing subpages.
  DEVICE_SELECTION_PAGE: 'deviceSelectionPage',
  DEVICE_REQUEST_CODE_PAGE: 'deviceRequestCodePage',
};

/**
 * @implements {chromeos.bluetoothConfig.mojom.BluetoothDiscoveryDelegateInterface}
 * @implements {chromeos.bluetoothConfig.mojom.DevicePairingDelegateInterface}
 * @polymer
 */
export class SettingsBluetoothPairingUiElement extends PolymerElement {
  static get is() {
    return 'bluetooth-pairing-ui';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Id of the currently selected Bluetooth pairing subpage.
       * @private {!BluetoothPairingSubpageId}
       */
      selectedPageId_: {
        type: String,
        value: BluetoothPairingSubpageId.DEVICE_SELECTION_PAGE,
      },

      /**
       * @private {Array<!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties>}
       */
      discoveredDevices_: {
        type: Array,
        value: [],
      },

      /**
       * @private {?chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
       */
      devicePendingPairing_: {
        type: Object,
        value: null,
      },

      /**
       * Used to access |BluetoothPairingSubpageId| type in HTML.
       * @private {!BluetoothPairingSubpageId}
       */
      SubpageId: {
        type: Object,
        value: BluetoothPairingSubpageId,
      },
    };
  }

  constructor() {
    super();
    /**
     * @private {!chromeos.bluetoothConfig.mojom.BluetoothDiscoveryDelegateReceiver}
     */
    this.bluetoothDiscoveryDelegateReceiver_ =
        new chromeos.bluetoothConfig.mojom.BluetoothDiscoveryDelegateReceiver(
            this);

    /**
     * @private {?chromeos.bluetoothConfig.mojom.DevicePairingHandlerInterface}
     */
    this.devicePairingHandler_;

    /**
     * @private {?chromeos.bluetoothConfig.mojom.DevicePairingDelegateReceiver}
     */
    this.pairingDelegateReceiver_ = null;
  }

  ready() {
    super.ready();
    getBluetoothConfig().startDiscovery(
        this.bluetoothDiscoveryDelegateReceiver_.$.bindNewPipeAndPassRemote());
  }

  /** @override */
  onDiscoveredDevicesListChanged(discoveredDevices) {
    this.discoveredDevices_ = discoveredDevices;
  }

  /** @override */
  onBluetoothDiscoveryStarted(handler) {
    this.devicePairingHandler_ = handler;
  }

  /** @override */
  onBluetoothDiscoveryStopped() {
    // TODO(crbug.com/1010321): Implement this function.
  }

  /**
   * @param {!CustomEvent<!{device:
   *     chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}>} event
   * @private
   */
  onPairDevice_(event) {
    // Pairing delegate should only be available after call to pair device
    // is made. This delegate is set to null after pair request is made and
    // returned, allowing for multiple pairing events in the same discovery
    // session, but only one pairing event at a time.
    assert(!this.pairingDelegateReceiver_);

    this.pairingDelegateReceiver_ =
        new chromeos.bluetoothConfig.mojom.DevicePairingDelegateReceiver(this);

    // TODO(crbug.com/1010321): Add test for |devicePendingPairing_| when
    // request code page UI is added.
    this.devicePendingPairing_ = event.detail.device;
    assert(this.devicePendingPairing_);

    this.devicePairingHandler_
        .pairDevice(
            this.devicePendingPairing_.id,
            this.pairingDelegateReceiver_.$.bindNewPipeAndPassRemote())
        .then(result => {
          this.handlePairDeviceResult_(result.result);
        });
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.PairingResult} result
   * @private
   */
  handlePairDeviceResult_(result) {
    this.pairingDelegateReceiver_ = null;
    this.devicePendingPairing_ = null;

    if (result === chromeos.bluetoothConfig.mojom.PairingResult.kSuccess) {
      this.dispatchEvent(new CustomEvent('finished', {
        bubbles: true,
        composed: true,
      }));
      return;
    }
    // TODO(crbug.com/1010321): Pass pairing result to child elements and add
    // test for pairing failure.
  }

  /** @override */
  async requestPinCode() {
    // TODO(crbug.com/1010321): Implement this function.
    return {pinCode: ''};
  }

  /** @override */
  async requestPasskey() {
    // TODO(crbug.com/1010321): Implement this function.
    return {passkey: ''};
  }

  /** @override */
  displayPinCode(pinCode, handler) {
    // TODO(crbug.com/1010321): Create keyEnterHandler and implement this
    // function.
  }

  /** @override */
  displayPasskey(passkey, handler) {
    // TODO(crbug.com/1010321): Create keyEnterHandler and implement this
    // function.
  }

  /** @override */
  confirmPasskey(passkey) {
    // TODO(crbug.com/1010321): Implement this function.
  }

  /** @override */
  authorizePairing() {
    // TODO(crbug.com/1010321): Implement this function.
  }

  /**
   * @param {!BluetoothPairingSubpageId} subpageId
   * @return {boolean}
   * @private
   */
  shouldShowSubpage_(subpageId) {
    return this.selectedPageId_ === subpageId;
  }
}

customElements.define(
    SettingsBluetoothPairingUiElement.is, SettingsBluetoothPairingUiElement);
