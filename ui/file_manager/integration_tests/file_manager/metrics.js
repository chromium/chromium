// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests that enum metrics are recorded correctly.
 */

import {createTestFile, ENTRIES, getHistogramCount, getHistogramSum, RootPath} from '../test_util.js';
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

// Test that the DirectoryListLoad UMA is appropriately recorded and the
// variance is taken into consideration (+/-20%).
testcase.metricsRecordDirectoryListLoad = async () => {
  const createEntries = numEntries => {
    const entries = [];
    for (let i = 0; i < numEntries; i++) {
      const testFile = createTestFile('file-' + i + '.txt');
      entries.push(testFile);
    }
    return entries;
  };

  const entries = createEntries(100);

  // Open Files app on Downloads with 10 files loaded.
  // Expect a non-zero load time in the appropriate histogram.
  let appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, entries.slice(0, 10), []);
  const tenFilesSum =
      await getHistogramSum('FileBrowser.DirectoryListLoad.my_files.10');
  chrome.test.assertTrue(
      tenFilesSum > 0, 'Load time for 10 files must exceed 0');
  await remoteCall.closeWindowAndWait(appId);

  // Open Files app on Downloads with 27 files loaded.
  // Histogram sum is cumulative so given 27 falls outside the buckets (and
  // their tolerance) the sum should not increase as no load time will be
  // recorded.
  appId = await setupAndWaitUntilReady(
      RootPath.DOWNLOADS, entries.slice(0, 27), []);
  const histogramSum =
      await getHistogramSum('FileBrowser.DirectoryListLoad.my_files.10');
  chrome.test.assertEq(
      tenFilesSum, histogramSum,
      'Load time for 27 files must equal same load time as previous');
  await remoteCall.closeWindowAndWait(appId);

  // Open Files app on Downloads with 100 files loaded.
  // Expect a non-zero load time in the appropriate histogram.
  appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, entries, []);
  const hundredFilesSum =
      await getHistogramSum('FileBrowser.DirectoryListLoad.my_files.100');
  chrome.test.assertTrue(
      hundredFilesSum > 0, 'Load time for 100 files must exceed 0');
  await remoteCall.closeWindowAndWait(appId);
};
