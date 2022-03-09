// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * UI element used to display Bluetooth device icon.
 */

import './bluetooth_icons.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {hasDefaultImage} from './bluetooth_utils.js';

/** @polymer */
export class SettingsBluetoothIconElement extends PolymerElement {
  static get is() {
    return 'bluetooth-icon';
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
    };
  }

  /**
   * @return {string}
   * @private
   */
  getIcon_() {
    if (!this.device) {
      return 'default';
    }

    const deviceType = chromeos.bluetoothConfig.mojom.DeviceType;
    switch (this.device.deviceType) {
      case deviceType.kComputer:
        return 'computer';
      case deviceType.kPhone:
        return 'phone';
      case deviceType.kHeadset:
        return 'headset';
      case deviceType.kVideoCamera:
        return 'video-camera';
      case deviceType.kGameController:
        return 'game-controller';
      case deviceType.kKeyboard:
        return 'keyboard';
      case deviceType.kMouse:
        return 'mouse';
      case deviceType.kTablet:
        return 'tablet';
      default:
        return 'default';
    }
  }

  /**
   * @return {boolean}
   * @private
   */
  hasDefaultImage_() {
    return hasDefaultImage(this.device);
  }

  /**
   * @return {string}
   * @private
   */
  getDefaultImageSrc_() {
    if (!this.hasDefaultImage_()) {
      return '';
    }
    return this.device.imageInfo.defaultImageUrl.url;
  }
}
customElements.define(
    SettingsBluetoothIconElement.is, SettingsBluetoothIconElement);
