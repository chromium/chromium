// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Main entry point for background page, it replaces the
 * background_scripts.js.
 */

import './metrics_start.m.js';
import '../../../base/js/error_counter.m.js';
import './background.m.js';
import './test_util.m.js';
