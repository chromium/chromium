// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 *
 * @suppress {uselessCode} Temporary suppress because of the line exporting.
 */

// clang-format off
// #import * as wrappedMetricsBase from './metrics_base.m.js'; const {metricsBase} = wrappedMetricsBase
// clang-format on

// eslint-disable-next-line no-var
var metrics = metrics || metricsBase;

/**
 * Convert a short metric name to the full format.
 *
 * @param {string} name Short metric name.
 * @return {string} Full metric name.
 * @override
 * @private
 */
metrics.convertName_ = name => {
  return 'FileBrowser.' + name;
};

// eslint-disable-next-line semi,no-extra-semi
/* #export */ {metrics};
