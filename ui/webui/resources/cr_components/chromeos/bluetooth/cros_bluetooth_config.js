// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cros_bluetooth_config.mojom-lite.js depends on url.mojom.Url.
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// TODO(crbug.com/1010321): Use cros_bluetooth_config.mojom-webui.js instead
// as non-module JS is deprecated.
import 'chrome://resources/mojo/chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-lite.js';

/**
 * @fileoverview
 * Wrapper for CrosBluetoothConfig that provides the ability to inject a fake
 * CrosBluetoothConfig implementation for tests.
 */

/** @type {?chromeos.bluetoothConfig.mojom.CrosBluetoothConfigInterface} */
let bluetoothConfig = null;

/**
 * @param {?chromeos.bluetoothConfig.mojom.CrosBluetoothConfigInterface}
 *     testBluetoothConfig The CrosBluetoothConfig implementation used for
 *                         testing. Passing null reverses the override.
 */
export function setBluetoothConfigForTesting(testBluetoothConfig) {
  bluetoothConfig = testBluetoothConfig;
}

/**
 * @return {!chromeos.bluetoothConfig.mojom.CrosBluetoothConfigInterface}
 */
export function getBluetoothConfig() {
  if (bluetoothConfig) {
    return bluetoothConfig;
  }

  bluetoothConfig =
      chromeos.bluetoothConfig.mojom.CrosBluetoothConfig.getRemote();
  return bluetoothConfig;
}