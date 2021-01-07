// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Metrics calls to start measurement of script loading.  Include
 * this as the first script in background_scripts.js (i.e. after the common
 * scripts that define the metrics namespace).
 */

metrics.startInterval('Load.BackgroundScript');
