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
import {assert, assertNotReached} from '../../../js/assert.m.js';
import {PairingAuthType} from './bluetooth_types.js';
import {getBluetoothConfig} from './cros_bluetooth_config.js';

/** @enum {string} */
const BluetoothPairingSubpageId = {
  // TODO(crbug.com/1010321): Add missing bluetooth pairing subpages.
  DEVICE_SELECTION_PAGE: 'deviceSelectionPage',
  DEVICE_REQUEST_CODE_PAGE: 'deviceRequestCodePage',
};

/**
 * @typedef {{
 *  resolve: ?function(string),
 *  reject: ?function(),
 * }}
 */
let RequestCodeCallback;

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

      /** @private {?PairingAuthType} */
      pairingAuthType_: {
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

    /** @private {?RequestCodeCallback} */
    this.requestCodeCallback_ = null;
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
    this.devicePendingPairing_ = null;
    this.pairingDelegateReceiver_.$.close();
    this.pairingDelegateReceiver_ = null;

    if (result === chromeos.bluetoothConfig.mojom.PairingResult.kSuccess) {
      this.dispatchEvent(new CustomEvent('finished', {
        bubbles: true,
        composed: true,
      }));
      return;
    }

    this.selectedPageId_ = BluetoothPairingSubpageId.DEVICE_SELECTION_PAGE;
    // TODO(crbug.com/1010321): Pass pairing result to subpages.
  }

  /** @override */
  requestPinCode() {
    return this.requestCode_(PairingAuthType.REQUEST_PIN_CODE);
  }

  /** @override */
  requestPasskey() {
    return this.requestCode_(PairingAuthType.REQUEST_PASSKEY);
  }

  /**
   * @param {!PairingAuthType} authType
   * @return {!Promise<{pinCode: !string}> | !Promise<{passkey: !string}>}
   * @private
   */
  requestCode_(authType) {
    this.pairingAuthType_ = authType;
    this.selectedPageId_ = BluetoothPairingSubpageId.DEVICE_REQUEST_CODE_PAGE;

    this.requestCodeCallback_ = {
      reject: null,
      resolve: null,
    };

    const promise = new Promise((resolve, reject) => {
      this.requestCodeCallback_.resolve = (code) => {
        if (authType === PairingAuthType.REQUEST_PIN_CODE) {
          resolve({'pinCode': code});
          return;
        }

        if (authType === PairingAuthType.REQUEST_PASSKEY) {
          resolve({'passkey': code});
          return;
        }

        assertNotReached();
      };
      this.requestCodeCallback_.reject = reject;
    });

    return promise;
  }

  /**
   * @param {!CustomEvent<!{code: string}>} event
   * @private
   */
  onRequestCodeEntered_(event) {
    event.stopPropagation();
    assert(this.pairingAuthType_);
    assert(this.requestCodeCallback_.resolve);
    this.requestCodeCallback_.resolve(event.detail.code);
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

  /**
   * @param {!Event} event
   * @private
   */
  onCancelClick_(event) {
    event.stopPropagation();
    if (this.pairingDelegateReceiver_) {
      this.pairingDelegateReceiver_.$.close();
    }

    // Canceling from any page other than |DEVICE_SELECTION_PAGE| should
    // return back to |DEVICE_SELECTION_PAGE|. This case is handled when
    // pairDevice promise is returned in handlePairDeviceResult_().
    // pairDevice promise is returned when close() is called above. If we are
    // on |DEVICE_SELECTION_PAGE|, canceling closses pairing dialog.
    if (this.selectedPageId_ ===
        BluetoothPairingSubpageId.DEVICE_SELECTION_PAGE) {
      this.dispatchEvent(new CustomEvent('finished', {
        bubbles: true,
        composed: true,
      }));
      return;
    }

    if (this.requestCodeCallback_) {
      // |requestCodeCallback_| promise is held by FakeDevicePairingHandler
      // in test. This does not get resolved for the test case where user
      // cancels request while in request code page. Calling reject is
      // necessary here to make sure the promise is resolved.
      this.requestCodeCallback_.reject();
    }
  }
}

customElements.define(
    SettingsBluetoothPairingUiElement.is, SettingsBluetoothPairingUiElement);
