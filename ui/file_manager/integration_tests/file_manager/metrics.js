// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests that enum metrics are recorded correctly.
 */

'use strict';

(() => {
  testcase.metricsRecordEnum = async () => {
    const appId = null;
    const histogramName = 'Foo';
    const fullHistogramName = `FileBrowser.${histogramName}`;
    const validValues = ['a', 'b', 'c'];
    const reports = [];

    // Record each enumerator once.
    for (const value of validValues) {
      reports.push(remoteCall.callRemoteTestUtil(
          'recordEnumMetric', appId, [histogramName, value, validValues]));
    }
    await Promise.all(reports);

    // Each bucket should contain exactly one sample.
    for (let i = 0; i < validValues.length; ++i) {
      chrome.test.assertEq(1, await getHistogramCount(fullHistogramName, i));
    }
  };
})();
