// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://test/chai_assert.js';

import {installMockChrome, MockCommandLinePrivate} from '../../../base/js/mock_chrome.m.js';
import {VolumeManagerCommon} from '../../../base/js/volume_manager_types.m.js';
import {metrics} from '../../common/js/metrics.m.js';

import {MountMetrics} from './mount_metrics.m.js';


/** @type {!MountMetrics} */
let mountMetrics;

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

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
        }
      }
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


  mountMetrics = new MountMetrics();
}

/**
 * Tests mounting a file system provider where the providerId is not known to
 * mount metrics.
 */
export function testMountUnknownProvider() {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      volumeType: VolumeManagerCommon.VolumeType.PROVIDED,
      providerId: 'fubar',
    }
  });

  assertEquals(metrics.calledName, 'FileSystemProviderMounted');
  assertEquals(metrics.calledValue, 0);
}

/**
 * Tests mounting Zip Archiver file system provider.
 */
export function testMountZipArchiver() {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      volumeType: VolumeManagerCommon.VolumeType.PROVIDED,
      providerId: 'dmboannefpncccogfdikhmhpmdnddgoe',
    }
  });

  assertEquals(metrics.calledName, 'FileSystemProviderMounted');
  assertEquals(metrics.calledValue, 15);
}
