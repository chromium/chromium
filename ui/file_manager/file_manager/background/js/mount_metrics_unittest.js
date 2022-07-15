// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {metrics} from '../../common/js/metrics.js';
import {installMockChrome, MockCommandLinePrivate} from '../../common/js/mock_chrome.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {MountMetrics} from './mount_metrics.js';

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

/**
 * Version of util.isSwaEnabled to restore at the end of the test.
 * @type {!function(): boolean}
 */
let restoreIsSwaEnabled;

/**
 * The output of util.isSwaEnabled for testing.
 * @type {boolean}
 */
let swaEnabledState = false;

/**
 * A sample event used in all tests.
 * @type {!ChromeEvent}
 */
const mountCompletedTestEvent = /** @type {!ChromeEvent} */ ({
  eventType: 'mount',
  status: 'success',
  volumeMetadata: {
    volumeType: VolumeManagerCommon.VolumeType.PROVIDED,
    providerId: 'fubar',
  },
});

// Set up the test components.
export function setUp() {
  mockChrome = {
    fileManagerPrivate: {
      onMountCompletedListeners_: [],
      onMountCompleted: {
        addListener: function(listener) {
          mockChrome.fileManagerPrivate.onMountCompletedListeners_.push(
              listener);
        },
        dispatchEvent: function(event) {
          mockChrome.fileManagerPrivate.onMountCompletedListeners_.forEach(
              listener => {
                listener(event);
              });
        },
      },
    },
  };
  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  // Mock metrics.
  metrics.calledName = '';
  metrics.calledValue = '';

  /**
   * @param {string} name Metric name.
   * @param {*} value Enum value.
   * @param {Array<*>|number=} opt_validValues .
   */
  metrics.recordEnum = function(name, value, opt_validValues) {
    metrics.calledName = name;
    metrics.calledValue = value;
  };

  window.isSWA = false;
  restoreIsSwaEnabled = util.isSwaEnabled;
  util.isSwaEnabled = () => swaEnabledState;
}

export function tearDown() {
  util.isSwaEnabled = restoreIsSwaEnabled;
}

/**
 * Tests mounting a file system provider where the providerId is not known to
 * mount metrics.
 */
export function testMountUnknownProvider() {
  new MountMetrics();

  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent(
      mountCompletedTestEvent);

  assertEquals(metrics.calledName, 'FileSystemProviderMounted');
  assertEquals(metrics.calledValue, 0);
}

/**
 * Tests when the SWA is enabled, the mount metrics are not recorded. Due to the
 * background code existing in every foreground window in the SWA context, this
 * will lead to duplicate mount metrics so don't record them.
 */
export function testMetricsNotRecordedWhenSwaEnabled() {
  window.isSWA = true;
  swaEnabledState = true;
  new MountMetrics();

  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent(
      mountCompletedTestEvent);

  assertEquals(metrics.calledName, '');
  assertEquals(metrics.calledValue, '');
}

/**
 * Test when the SWA flag is enabled but the Chrome app is running, the Chrome
 * app version still doesn't record any UMAs.
 */
export function testMetricsNotRecordedForChromeAppWhenSwaEnabled() {
  window.isSWA = false;
  swaEnabledState = true;
  new MountMetrics();

  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent(
      mountCompletedTestEvent);

  assertEquals(metrics.calledName, '');
  assertEquals(metrics.calledValue, '');
}
