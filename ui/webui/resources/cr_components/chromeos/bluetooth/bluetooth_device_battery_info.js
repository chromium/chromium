// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * View displaying Bluetooth device battery information.
 */

import '../../../cr_elements/shared_style_css.m.js';
import './bluetooth_battery_icon_percentage.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {BatteryType} from './bluetooth_types.js';
import {getBatteryPercentage, hasAnyDetailedBatteryInfo} from './bluetooth_utils.js';

/** @polymer */
export class BluetoothDeviceBatteryInfoElement extends PolymerElement {
  static get is() {
    return 'bluetooth-device-battery-info';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!BluetoothDeviceProperties}
       */
      device: {
        type: Object,
      },

      /**
       * Enum used as an ID for specific UI elements.
       * A BatteryType is passed between html and JS for
       * certain UI elements to determine their state.
       *
       * @type {!BatteryType}
       */
      BatteryType: {
        type: Object,
        value: BatteryType,
      },

      /** @protected {boolean} */
      showMultipleBatteries_: {
        type: Boolean,
        computed: 'computeShowMultipleBatteries_(device)',
      },
    };
  }

  /**
   * @param {!BluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @private
   */
  computeShowMultipleBatteries_(device) {
    return hasAnyDetailedBatteryInfo(device);
  }

  /**
   * @param {!BluetoothDeviceProperties}
   *     device
   * @param {!BatteryType} batteryType
   * @return {boolean}
   * @private
   */
  shouldShowBattery_(device, batteryType) {
    return getBatteryPercentage(device, batteryType) !== undefined;
  }
}

customElements.define(
    BluetoothDeviceBatteryInfoElement.is, BluetoothDeviceBatteryInfoElement);
