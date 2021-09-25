// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via PIN or PASSKEY is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '../../../cr_elements/shared_style_css.m.js';
import '../../../cr_elements/cr_input/cr_input.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ButtonBarState, ButtonState} from './bluetooth_types.js';
import {mojoString16ToString} from './bluetooth_utils.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingRequestCodePageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothRequestCodePageElement extends
    SettingsBluetoothPairingRequestCodePageElementBase {
  static get is() {
    return 'bluetooth-pairing-request-code-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {?chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
       */
      device: {
        type: Object,
        value: null,
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        computed: 'computeButtonBarState_(pinCode_)',
      },

      /** @private {string} */
      pinCode_: {
        type: String,
        value: '',
      }
    };
  }

  /**
   * @private
   * @return {string}
   */
  getMessage_() {
    return this.i18n('bluetoothEnterPin', this.getDeviceName_());
  }

  /**
   * @private
   * @return {string}
   */
  getDeviceName_() {
    if (!this.device) {
      return '';
    }

    return mojoString16ToString(this.device.publicName);
  }

  /**
   * @return {!ButtonBarState}
   * @private
   */
  computeButtonBarState_() {
    const pairButtonState =
        !this.pinCode_ ? ButtonState.DISABLED : ButtonState.ENABLED;

    return {
      cancel: ButtonState.ENABLED,
      pair: pairButtonState,
    };
  }
}

customElements.define(
    SettingsBluetoothRequestCodePageElement.is,
    SettingsBluetoothRequestCodePageElement);
