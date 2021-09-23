// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element to show a list of discovered Bluetooth devices and initiate
 * pairing to a device.
 */
import './bluetooth_base_page.js';
import './bluetooth_pairing_device_item.js';
import '../../../cr_elements/shared_style_css.m.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '../localized_link/localized_link.js';

import {CrScrollableBehavior, CrScrollableBehaviorInterface} from '//resources/cr_elements/cr_scrollable_behavior.m.js';
import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ButtonBarState, ButtonState} from './bluetooth_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {CrScrollableBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingDeviceSelectionPageElementBase =
    mixinBehaviors([CrScrollableBehavior, I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingDeviceSelectionPageElement extends
    SettingsBluetoothPairingDeviceSelectionPageElementBase {
  static get is() {
    return 'bluetooth-pairing-device-selection-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {!Array<!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties>}
       */
      devices: {
        type: Array,
        value: [],
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      }
    };
  }

  /**
   * @private
   * @return {boolean}
   */
  shouldShowDeviceList_() {
    return this.devices && this.devices.length > 0;
  }

  /**
   * @private
   * @return {string}
   */
  getDeviceListTitle_() {
    if (this.shouldShowDeviceList_()) {
      return this.i18n('bluetoothAvailableDevices');
    }

    return this.i18n('bluetoothNoAvailableDevices');
  }
}

customElements.define(
    SettingsBluetoothPairingDeviceSelectionPageElement.is,
    SettingsBluetoothPairingDeviceSelectionPageElement);
