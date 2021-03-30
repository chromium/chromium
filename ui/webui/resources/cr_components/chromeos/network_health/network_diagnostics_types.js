// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_health/public/mojom/network_diagnostics.mojom-lite.js';
// clang-format on

/**
 * @fileoverview
 * This file contains shared types for the network diagnostics components.
 */

/**
 * A routine response from the Network Diagnostics mojo service.
 * @typedef {{
 *   verdict: chromeos.networkDiagnostics.mojom.RoutineVerdict,
 * }}
 * RoutineResponse can optionally have a `problems` field, which is an array of
 * enums relevant to the routine run. Unfortunately the closure compiler cannot
 * handle optional object fields.
 */
/* #export */ let RoutineResponse;

/**
 * A network diagnostics routine. Holds descriptive information about the
 * routine, and it's transient state.
 * @typedef {{
 *   name: string,
 *   type: !RoutineType,
 *   group: !RoutineGroup,
 *   func: function(),
 *   running: boolean,
 *   resultMsg: string,
 *   result: ?RoutineResponse,
 * }}
 */
/* #export */ let Routine;

/**
 * Definition for all Network diagnostic routine types. This enum is intended
 * to be used as an index in an array of routines.
 * @enum {number}
 */
/* #export */ const RoutineType = {
  LAN_CONNECTIVITY: 0,
  SIGNAL_STRENGTH: 1,
  GATEWAY_PING: 2,
  SECURE_WIFI: 3,
  DNS_RESOLVER: 4,
  DNS_LATENCY: 5,
  DNS_RESOLUTION: 6,
  HTTP_FIREWALL: 7,
  HTTPS_FIREWALL: 8,
  HTTPS_LATENCY: 9,
  CAPTIVE_PORTAL: 10,
  VIDEO_CONFERENCING: 11,
};

/**
 * Definition for different groups of network routines.
 * @enum {number}
 */
/* #export */ const RoutineGroup = {
  CONNECTION: 0,
  WIFI: 1,
  PORTAL: 2,
  GATEWAY: 3,
  FIREWALL: 4,
  DNS: 5,
  GOOGLE_SERVICES: 6,
};

/* #export */ const Icons = {
  TEST_FAILED: 'test_failed.png',
  TEST_NOT_RUN: 'test_not_run.png',
  TEST_PASSED: 'test_passed.png'
};
