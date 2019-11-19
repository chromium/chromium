// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {!MountMetrics} */
let mountMetrics;

/**
 * Mock chrome APIs.
 * @type {!Object}
 */
let mockChrome;

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  calledName: '',
  calledValue: '',
  recordEnum: function(name, value, opt_validValues) {
    window.metrics.calledName = name;
    window.metrics.calledValue = value;
  },
};

// Set up the test components.
function setUp() {
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
    }
  };
  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  mountMetrics = new MountMetrics();
}

/**
 * Tests mounting a file system provider where the providerId is not known to
 * mount metrics.
 */
function testMountUnknownProvider() {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      volumeType: VolumeManagerCommon.VolumeType.PROVIDED,
      providerId: 'fubar',
    }
  });

  assertEquals(window.metrics.calledName, 'FileSystemProviderMounted');
  assertEquals(window.metrics.calledValue, 0);
}

/**
 * Tests mounting Zip Archiver file system provider.
 */
function testMountZipArchiver() {
  mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
    eventType: 'mount',
    status: 'success',
    volumeMetadata: {
      volumeType: VolumeManagerCommon.VolumeType.PROVIDED,
      providerId: 'dmboannefpncccogfdikhmhpmdnddgoe',
    }
  });

  assertEquals(window.metrics.calledName, 'FileSystemProviderMounted');
  assertEquals(window.metrics.calledValue, 15);
}
