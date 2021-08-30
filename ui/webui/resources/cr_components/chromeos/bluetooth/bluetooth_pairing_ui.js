// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Root UI element for Bluetooth pairing dialog.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './bluetooth_base_page.js';
import './bluetooth_pairing_device_selection_page.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @enum {string} */
const BluetoothPairingSubpageId = {
  // TODO(crbug.com/1010321): Add missing bluetooth pairing subpages.
  DEVICE_SELECTION_PAGE: 'deviceSelectionPage',
};

/** @polymer */
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
    };
  }
}

customElements.define(
    SettingsBluetoothPairingUiElement.is, SettingsBluetoothPairingUiElement);
