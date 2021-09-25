// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element to show a list of discovered Bluetooth devices and initiate
 * pairing to a device.
 */
import '../../../cr_elements/shared_style_css.m.js';
import './bluetooth_icon.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {mojoString16ToString} from './bluetooth_utils.js';

/** @polymer */
export class SettingsBluetoothPairingDeviceItemElement extends PolymerElement {
  static get is() {
    return 'bluetooth-pairing-device-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
       */
      device: Object,
    };
  }

  /**
   * @return {string}
   * @private
   */
  getDeviceName_() {
    if (!this.device) {
      return '';
    }

    return mojoString16ToString(this.device.publicName);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onSelected_(event) {
    this.dispatchPairDeviceEvent_();
    event.stopPropagation();
  }

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    if (event.key !== 'Enter' && event.key !== ' ') {
      return;
    }

    this.dispatchPairDeviceEvent_();
    event.stopPropagation();
  }

  /** @private */
  dispatchPairDeviceEvent_() {
    this.dispatchEvent(new CustomEvent('pair-device', {
      bubbles: true,
      composed: true,
      detail: {device: this.device},
    }));
  }
}

customElements.define(
    SettingsBluetoothPairingDeviceItemElement.is,
    SettingsBluetoothPairingDeviceItemElement);
