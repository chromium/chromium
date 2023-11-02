// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility methods for accessing chrome.metricsPrivate API.
 *
 * To be included as a first script in main.html
 */

import {metricsBase} from './metrics_base.js';

const metrics = metricsBase;

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

export {metrics};
