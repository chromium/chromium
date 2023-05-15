// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as tslib from 'chrome://resources/mwc/tslib/tslib.js';

/**
 * This file serves as a shim to tslib. Using experimental features like
 * decorator will make TS generates compiled JS code like "import 'tslib'",
 * but our existing build toolchain can't handle that import correctly. To
 * mitigate that, we use "noEmitHelpers: true" in the tsconfig to make sure
 * it won't generate "import 'tslib'", but this configuration requires the
 * functions from tslib are available in the global space, hence the assignment
 * below.
 *
 * Note: for any functions we expose here, we also need to add function
 * type declaration to closure type externs in app_window_common.js.
 */

(globalThis as any).__decorate = tslib.__decorate;
