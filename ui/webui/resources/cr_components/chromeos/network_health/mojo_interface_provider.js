// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
import 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-lite.js';

/**
 * @fileoverview
 * This file contains the mojo interface for the network diagnostics service and
 * methods to override the service for testing.
 */

/**
 * @type
 *     {?chromeos.networkDiagnostics.mojom.NetworkDiagnosticsRoutinesInterface}
 */
let networkDiagnosticsService = null;

/**
 * @param
 *     {!chromeos.networkDiagnostics.mojom.NetworkDiagnosticsRoutinesInterface}
 *     testNetworkDiagnosticsService
 */
export function setNetworkDiagnosticsServiceForTesting(
    testNetworkDiagnosticsService) {
  networkDiagnosticsService = testNetworkDiagnosticsService;
}

/**
 * @return
 *     {!chromeos.networkDiagnostics.mojom.NetworkDiagnosticsRoutinesInterface}
 */
export function getNetworkDiagnosticsService() {
  if (networkDiagnosticsService) {
    return networkDiagnosticsService;
  }

  networkDiagnosticsService =
      chromeos.networkDiagnostics.mojom.NetworkDiagnosticsRoutines.getRemote();
  return networkDiagnosticsService;
}
