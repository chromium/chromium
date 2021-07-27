// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://test/chai_assert.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Banner} from '../../externs/banner.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {isAllowedVolume} from './banner_controller.js';

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
