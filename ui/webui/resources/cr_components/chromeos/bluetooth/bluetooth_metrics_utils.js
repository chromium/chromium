// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * These values are persisted to logs and should not be renumbered or re-used.
 * See tools/metrics/histograms/enums.xml.
 * @enum {number}
 */
export const BluetoothUiSurface = {
  SETTINGS_DEVICE_LIST_SUBPAGE: 0,
  SETTINGS_DEVICE_DETAIL_SUBPAGE: 1,
  SETTINGS_PAIRING_DIALOG: 2,
  BLUETOOTH_QUICK_SETTINGS: 3,
  PAIRING_DIALOG: 4,
  PAIRED_NOTIFICATION: 5,
  CONNECTION_TOAST: 6,
  DISCONNECTED_TOAST: 7,
  OOBE_HID_DETECTION: 8,
};

/**
 * Records metric indicating that |uiSurface| was displayed to the user.
 * @param {!BluetoothUiSurface} uiSurface Bluetooth UI surface displayed.
 */
export function recordBluetoothUiSurfaceMetrics(uiSurface) {
  chrome.metricsPrivate.recordEnumerationValue(
      'Bluetooth.ChromeOS.UiSurfaceDisplayed', uiSurface,
      Object.keys(BluetoothUiSurface).length);
}