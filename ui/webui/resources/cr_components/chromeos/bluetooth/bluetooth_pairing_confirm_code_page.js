// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element shown when authentication via confirm passkey is required
 * during Bluetooth device pairing.
 */

import './bluetooth_base_page.js';
import '../../../cr_elements/shared_style_css.m.js';
import '../../../cr_elements/cr_input/cr_input.m.js';

import {I18nBehavior, I18nBehaviorInterface} from '//resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ButtonBarState, ButtonState} from './bluetooth_types.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 */
const SettingsBluetoothPairingConfirmCodePageElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement);

/** @polymer */
export class SettingsBluetoothPairingConfirmCodePageElement extends
    SettingsBluetoothPairingConfirmCodePageElementBase {
  static get is() {
    return 'bluetooth-pairing-confirm-code-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {string} */
      code: {
        type: String,
        value: '',
      },

      /** @private {!ButtonBarState} */
      buttonBarState_: {
        type: Object,
        value: {
          cancel: ButtonState.ENABLED,
          pair: ButtonState.ENABLED,
        },
      }
    };
  }

  /**
   * @param {!Event} event
   * @private
   */
  onPairClicked_(event) {
    event.stopPropagation();
    this.dispatchEvent(new CustomEvent('confirm-code', {
      bubbles: true,
      composed: true,
    }));
  }
}

customElements.define(
    SettingsBluetoothPairingConfirmCodePageElement.is,
    SettingsBluetoothPairingConfirmCodePageElement);
