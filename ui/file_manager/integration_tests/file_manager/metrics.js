// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests that enum metrics are recorded correctly.
 */

import {ENTRIES, getHistogramCount, RootPath} from '../test_util.js';
import {testcase} from '../testcase.js';

import {setupAndWaitUntilReady} from './background.js';
import {remoteCall} from './background.js';

testcase.metricsRecordEnum = async () => {
  const appId = null;
  const histogramName = 'Foo';
  const fullHistogramName = `FileBrowser.${histogramName}`;
  const validValues = ['a', 'b', 'c'];
  const reports = [];

  // Open Files SWA.
  await setupAndWaitUntilReady(RootPath.DOWNLOADS);

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

testcase.metricsOpenSwa = async () => {
  // Open Files SWA:
  await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

  // Value basd on DialogType in file_manager/common/js/dialog_type.js:
  const FileDialogTypeValues = {
    FULL_PAGE: 5,
  };

  // Check that the UMA for SWA was incremented.
  chrome.test.assertEq(
      1,
      await getHistogramCount(
          'FileBrowser.SWA.Create', FileDialogTypeValues.FULL_PAGE));
};
