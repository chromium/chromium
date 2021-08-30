// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element to show a list of discovered Bluetooth devices and initiate
 * pairing to a device.
 */
import './bluetooth_base_page.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class SettingsBluetoothPairingDeviceSelectionPageUiElement extends
    PolymerElement {
  static get is() {
    return 'bluetooth-pairing-device-selection-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(
    SettingsBluetoothPairingDeviceSelectionPageUiElement.is,
    SettingsBluetoothPairingDeviceSelectionPageUiElement);
