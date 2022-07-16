// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * View displaying Bluetooth device battery information.
 */

import '../../../cr_elements/shared_style_css.m.js';
import './bluetooth_icons.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert.m.js';

import {getBatteryPercentage} from './bluetooth_utils.js';

/**
 * The threshold percentage where any battery percentage lower is considered
 * 'low battery'.
 * @type {number}
 */
const LOW_BATTERY_THRESHOLD_PERCENTAGE = 25;

/**
 * Ranges for each battery icon, where the value of the first index is the
 * minimum battery percentage in the range (inclusive), and the second index is
 * the maximum battery percentage in the range (inclusive).
 * @type {Array<Array<number>>}
 */
const BATTERY_ICONS_RANGES = [
  [0, 7], [8, 14], [15, 21], [22, 28], [29, 35], [36, 42], [43, 49], [50, 56],
  [57, 63], [64, 70], [71, 77], [78, 85], [86, 92], [93, 100]
];

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const BluetoothDeviceBatteryInfoElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class BluetoothDeviceBatteryInfoElement extends
    BluetoothDeviceBatteryInfoElementBase {
  static get is() {
    return 'bluetooth-device-battery-info';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
       */
      device: {
        type: Object,
      },

      /** @private {boolean} */
      isLowBattery_: {
        reflectToAttribute: true,
        type: Boolean,
        computed: 'computeIsLowBattery_(device)',
      }
    };
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @return {boolean}
   * @private
   */
  computeIsLowBattery_(device) {
    const batteryPercentage = getBatteryPercentage(device);
    if (batteryPercentage === undefined) {
      return false;
    }
    return batteryPercentage < LOW_BATTERY_THRESHOLD_PERCENTAGE;
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getBatteryPercentageString_(device) {
    const batteryPercentage = getBatteryPercentage(device);
    if (batteryPercentage === undefined) {
      return '';
    }

    return this.i18n(
        'bluetoothPairedDeviceItemBatteryPercentage', batteryPercentage);
  }

  /**
   * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
   *     device
   * @return {string}
   * @private
   */
  getBatteryIcon_(device) {
    const batteryPercentage = getBatteryPercentage(device);
    if (batteryPercentage === undefined) {
      return '';
    }

    // Range should always find a value because this element should not be
    // showing if batteryPercentage is out of bounds.
    const range = BATTERY_ICONS_RANGES.find(range => {
      return range[0] <= batteryPercentage && batteryPercentage <= range[1];
    });
    assert(
        !!range && range.length === 2, 'Battery percentage range is invalid');

    return 'bluetooth:battery-' + range[0] + '-' + range[1];
  }

  /** @return {boolean} */
  getIsLowBatteryForTest() {
    return this.isLowBattery_;
  }
}

customElements.define(
    BluetoothDeviceBatteryInfoElement.is, BluetoothDeviceBatteryInfoElement);