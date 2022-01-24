// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';

/**
 * Converts a JS string to mojoBase.mojom.String16 object.
 * @param {string} str
 * @return {!mojoBase.mojom.String16}
 */
export function stringToMojoString16(str) {
  const arr = [];
  for (let i = 0; i < str.length; i++) {
    arr[i] = str.charCodeAt(i);
  }
  return {data: arr};
}

/**
 * Converts mojoBase.mojom.String16 to a JS string.
 * @param {!mojoBase.mojom.String16} str16
 * @return {string}
 */
export function mojoString16ToString(str16) {
  return str16.data.map(ch => String.fromCodePoint(ch)).join('');
}

/**
 * @param {?chromeos.bluetoothConfig.mojom.PairedBluetoothDeviceProperties}
 *     device
 * @return {string}
 */
export function getDeviceName(device) {
  if (!device) {
    return '';
  }

  if (device.nickname) {
    return device.nickname;
  }

  return mojoString16ToString(device.deviceProperties.publicName);
}

/**
 * Returns the battery percentage of device, or undefined if device does
 * not exist, has no battery information, or the battery percentage is out of
 * bounds. Clients that call this method should explicitly check if the return
 * value is undefined to differentiate it from a return value of 0.
 * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
 *     device
 * @return {number|undefined}
 */
export function getBatteryPercentage(device) {
  if (!device) {
    return undefined;
  }

  const batteryInfo = device.batteryInfo;
  if (!batteryInfo || !batteryInfo.defaultProperties) {
    return undefined;
  }

  const batteryPercentage = batteryInfo.defaultProperties.batteryPercentage;
  if (batteryPercentage < 0 || batteryPercentage > 100) {
    return undefined;
  }

  return batteryPercentage;
}
