// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://test/chai_assert.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Banner} from '../../externs/banner.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {isAllowedVolume, isBelowThreshold} from './banner_controller.js';

/** @type {!Array<!Banner.AllowedVolumeType>} */
let allowedVolumeTypes = [];

/**
 * Returns a VolumeInfo with the type and id set.
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {string|null} volumeId
 * @returns {!VolumeInfo}
 */
function createAndSetVolumeInfo(volumeType, volumeId) {
  class FakeVolumeInfo {
    constructor() {
      this.volumeType = volumeType;
      this.volumeId = volumeId;
    }
  }

  return /** @type {!VolumeInfo} */ (new FakeVolumeInfo());
}

/**
 * Creates a chrome.fileManagerPrivate.MountPointSizeStats type.
 * @param {number} remainingSize
 * @returns {!chrome.fileManagerPrivate.MountPointSizeStats}
 */
function createRemainingSizeStats(remainingSize) {
  return {
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize,
  };
}

export function tearDown() {
  allowedVolumeTypes = [];
}

/**
 * Test when there are no allowed volume types.
 */
export function testNoAllowedVolumes() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS, null);

  assertFalse(isAllowedVolume(currentVolume, allowedVolumeTypes));
}

/**
 * Test when there is a single allowed volume type matching the current volume.
 */
export function testAllowedVolume() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS, null);
  allowedVolumeTypes = [{type: VolumeManagerCommon.VolumeType.DOWNLOADS}];

  assertTrue(isAllowedVolume(currentVolume, allowedVolumeTypes));
}

/**
 * Test when there are multiple volumes with one being the current volume.
 */
export function testMultipleAllowedVolumes() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS, null);
  allowedVolumeTypes = [
    {type: VolumeManagerCommon.VolumeType.DOWNLOADS},
    {type: VolumeManagerCommon.VolumeType.ANDROID_FILES}
  ];

  assertTrue(isAllowedVolume(currentVolume, allowedVolumeTypes));
}

/**
 * Test when there are multiple volumes but none match the current volume.
 */
export function testMultipleNoAllowedVolumes() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS, null);
  allowedVolumeTypes = [
    {type: VolumeManagerCommon.VolumeType.ARCHIVE},
    {type: VolumeManagerCommon.VolumeType.ANDROID_FILES}
  ];

  assertFalse(isAllowedVolume(currentVolume, allowedVolumeTypes));
}

/**
 * Test multiple identical volume types, with only one allowed volume id.
 */
export function testMultipleAllowedDocumentProviders() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  allowedVolumeTypes = [
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      id: 'provider_a',
    },
    {type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, id: 'provider_b'}
  ];

  assertTrue(isAllowedVolume(currentVolume, allowedVolumeTypes));
}

/**
 * Test multiple identical volume types, with none matching the current volume.
 */
export function testMultipleNoAllowedDocumentProviders() {
  const currentVolume = createAndSetVolumeInfo(
      VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  allowedVolumeTypes = [
    {
      type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER,
      id: 'provider_b',
    },
    {type: VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER, id: 'provider_c'}
  ];

  assertFalse(isAllowedVolume(currentVolume, allowedVolumeTypes));
}

/**
 * Test undefined types return false.
 */
export function testUndefinedThresholdAndSizeStats() {
  const testMinSizeThreshold = {
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  };
  const testMinRatioThreshold = {
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minSize: 0.1,
  };
  const testSizeStats = {
    totalSize: 20 * 1024 * 1024 * 1024,     // 20 GB
    remainingSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  };

  assertFalse(isBelowThreshold(undefined, undefined));
  assertFalse(isBelowThreshold(testMinSizeThreshold, undefined));
  assertFalse(isBelowThreshold(testMinRatioThreshold, undefined));
  assertFalse(isBelowThreshold(undefined, testSizeStats));
}

/**
 * Test that below, equal to and above the minSize threshold returns correct
 * results.
 */
export function testMinSizeReturnsCorrectly() {
  const createMinSizeThreshold = (minSize) => ({
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minSize,
  });

  // Remaining Size: 512 KB, Min Size: 1 GB
  assertTrue(isBelowThreshold(
      createMinSizeThreshold(1 * 1024 * 1024 * 1024 /* 1 GB */),
      createRemainingSizeStats(512 * 1024 * 1024 /* 512 KB */)));

  // Remaining Size: 1 GB, Min Size: 1 GB
  assertTrue(isBelowThreshold(
      createMinSizeThreshold(1 * 1024 * 1024 * 1024 /* 1 GB */),
      createRemainingSizeStats(1 * 1024 * 1024 * 1024 /* 1 GB */)));

  // Remaining Size: 2 GB, Min Size: 1 GB
  assertFalse(isBelowThreshold(
      createMinSizeThreshold(1 * 1024 * 1024 * 1024 /* 1 GB */),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));
}

/**
 * Test that below, equal to and above the minRatio threshold returns correct
 * results.
 */
export function testMinRatioReturnsCorrectly() {
  const createMinRatioThreshold = (minRatio) => ({
    type: VolumeManagerCommon.VolumeType.DOWNLOADS,
    minRatio,
  });

  // Remaining Size Ratio: 0.1, Threshold: 0.2
  assertTrue(isBelowThreshold(
      createMinRatioThreshold(0.2),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));

  // Remaining Size Ratio: 0.1, Threshold: 0.1
  assertTrue(isBelowThreshold(
      createMinRatioThreshold(0.1),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));

  // Remaining Size Ratio: 0.1, Threshold: 0.05
  assertFalse(isBelowThreshold(
      createMinRatioThreshold(0.05),
      createRemainingSizeStats(2 * 1024 * 1024 * 1024 /* 2 GB */)));
}
