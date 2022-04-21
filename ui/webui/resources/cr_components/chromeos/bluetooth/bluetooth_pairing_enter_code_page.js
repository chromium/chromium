// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via display passkey or PIN is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '../../../cr_elements/shared_style_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {BluetoothDeviceProperties} from 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {ButtonBarState, ButtonState} from './bluetooth_types.js';
import {mojoString16ToString} from './bluetooth_utils.js';

// Pairing passkey can be a maximum of 16 characters while pairing code a max
// of  6 digits. This is used to check that the passed code is less than or
// equal to the max possible value.
const MAX_CODE_LENGTH = 16;
/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingEnterCodeElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingEnterCodeElement extends
    SettingsBluetoothPairingEnterCodeElementBase {
  static get is() {
    return 'bluetooth-pairing-enter-code-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {?BluetoothDeviceProperties}
       */
      device: {
        type: Object,
        value: null,
      },

      /** @type {string} */
      code: {
        type: String,
        value: '',
      },

      /** @type {number} */
      numKeysEntered: {
        type: Number,
        value: 0,
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.HIDDEN,
        },
      },

      /**
       * Array representation of |code|.
       * @private {!Array<string>}
       */
      keys_: {
        type: Array,
        computed: 'computeKeys_(code)',
      }
    };
  }

  /**
   * @return {!Array<string>}
   * @private
   */
  computeKeys_() {
    if (!this.code) {
      return [];
    }

    assert(this.code.length <= MAX_CODE_LENGTH);
    return this.code.split('');
  }

  /**
   * @param {number} index
   * @return {string}
   */
  getKeyAt_(index) {
    return this.keys_[index];
  }

  /**
   * @param {number} index
   * @return {string}
   * @private
   */
  getKeyClass_(index) {
    if (!this.keys_ || !this.numKeysEntered) {
      return '';
    }

    if (index === this.numKeysEntered) {
      return 'next';
    } else if (index < this.numKeysEntered) {
      return 'typed';
    }

    return '';
  }

  /**
   * @return {string}
   * @private
   */
  getEnterClass_() {
    if (!this.keys_ || !this.numKeysEntered) {
      return '';
    }

    if (this.numKeysEntered >= this.keys_.length) {
      return 'next';
    }

    return '';
  }

  /**
   * @private
   * @return {string}
   */
  getMessage_() {
    return this.i18n('bluetoothPairingEnterKeys', this.getDeviceName_());
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
}

customElements.define(
    SettingsBluetoothPairingEnterCodeElement.is,
    SettingsBluetoothPairingEnterCodeElement);
