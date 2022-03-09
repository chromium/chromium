// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';

import {BatteryType} from './bluetooth_types.js';

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
 * Returns the battery percentage of the battery type of the device, or
 * undefined if device does not exist, has no battery information describing
 * the battery type, or the battery percentage is out of bounds. Clients that
 * call this method should explicitly check if the return value is undefined to
 * differentiate it from a return value of 0.
 * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
 *     device
 * @param {!BatteryType} batteryType
 * @return {number|undefined}
 */
export function getBatteryPercentage(device, batteryType) {
  if (!device) {
    return undefined;
  }

  const batteryInfo = device.batteryInfo;
  if (!batteryInfo) {
    return undefined;
  }

  let batteryProperties;
  switch (batteryType) {
    case BatteryType.DEFAULT:
      batteryProperties = batteryInfo.defaultProperties;
      break;
    case BatteryType.LEFT_BUD:
      batteryProperties = batteryInfo.leftBudInfo;
      break;
    case BatteryType.CASE:
      batteryProperties = batteryInfo.caseInfo;
      break;
    case BatteryType.RIGHT_BUD:
      batteryProperties = batteryInfo.rightBudInfo;
      break;
  }

  if (!batteryProperties) {
    return undefined;
  }

  const batteryPercentage = batteryProperties.batteryPercentage;
  if (batteryPercentage < 0 || batteryPercentage > 100) {
    return undefined;
  }

  return batteryPercentage;
}

/**
 * Returns true if the the device contains any multiple battery information.
 * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
 *     device
 * @return {boolean}
 */
export function hasAnyDetailedBatteryInfo(device) {
  return getBatteryPercentage(device, BatteryType.LEFT_BUD) !== undefined ||
      getBatteryPercentage(device, BatteryType.CASE) !== undefined ||
      getBatteryPercentage(device, BatteryType.RIGHT_BUD) !== undefined;
}

/**
 * Returns true if the device contains the default image URL.
 * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
 *     device
 * @return {boolean}
 */
export function hasDefaultImage(device) {
  return !!device.imageInfo && !!device.imageInfo.defaultImageUrl &&
      !!device.imageInfo.defaultImageUrl.url;
}

/**
 * Returns true if the device contains True Wireless Images.
 * @param {!chromeos.bluetoothConfig.mojom.BluetoothDeviceProperties}
 *     device
 * @return {boolean}
 */
export function hasTrueWirelessImages(device) {
  const imageInfo = device.imageInfo;
  if (!imageInfo) {
    return false;
  }

  const trueWirelessImages = imageInfo.trueWirelessImages;
  if (!trueWirelessImages) {
    return false;
  }

  // Only return true if all True Wireless Images are present.
  const leftBudImageUrl = trueWirelessImages.leftBudImageUrl;
  const rightBudImageUrl = trueWirelessImages.rightBudImageUrl;
  const caseImageUrl = trueWirelessImages.caseImageUrl;
  if (!leftBudImageUrl || !rightBudImageUrl || !caseImageUrl) {
    return false;
  }

  return !!leftBudImageUrl.url && !!rightBudImageUrl.url && !!caseImageUrl.url;
}
