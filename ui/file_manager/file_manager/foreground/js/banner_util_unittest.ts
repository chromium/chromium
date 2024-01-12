// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';

import {isAllowedVolume, isBelowThreshold} from './banner_controller.js';
import type {AllowedVolumeOrType, MinDiskThreshold} from './ui/banners/types.js';

let allowedVolumes: AllowedVolumeOrType[] = [];

/**
 * Returns a VolumeInfo with the type and id set.
 */
function createAndSetVolumeInfo(
    volumeType: VolumeType, volumeId: string|null = null) {
  class FakeVolumeInfo {
    volumeType: VolumeType;
    volumeId: string|null;
    constructor() {
      this.volumeType = volumeType;
      this.volumeId = volumeId;
    }
  }

  return new FakeVolumeInfo() as VolumeInfo;
}

/**
 * Creates a chrome.fileManagerPrivate.MountPointSizeStats type.
 */
function createRemainingSizeStats(remainingSize: number) {
  return {
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize,
  } as chrome.fileManagerPrivate.MountPointSizeStats;
}

export function tearDown() {
  allowedVolumes = [];
}

/**
 * Test when there are no allowed volume types.
 */
export function testNoAllowedVolumes() {
  const currentVolume = createAndSetVolumeInfo(VolumeType.DOWNLOADS);

  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test when there is a single allowed volume type matching the current volume.
 */
export function testAllowedVolume() {
  const currentVolume = createAndSetVolumeInfo(VolumeType.DOWNLOADS);
  allowedVolumes = [{type: VolumeType.DOWNLOADS}];

  assertTrue(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test when there are multiple volumes with one being the current volume.
 */
export function testMultipleAllowedVolumes() {
  const currentVolume = createAndSetVolumeInfo(VolumeType.DOWNLOADS);
  allowedVolumes = [
    {type: VolumeType.DOWNLOADS},
    {type: VolumeType.ANDROID_FILES},
  ];

  assertTrue(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test when there are multiple volumes but none match the current volume.
 */
export function testMultipleNoAllowedVolumes() {
  const currentVolume = createAndSetVolumeInfo(VolumeType.DOWNLOADS);
  allowedVolumes = [
    {type: VolumeType.ARCHIVE},
    {type: VolumeType.ANDROID_FILES},
  ];

  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test multiple identical volume types, with only one allowed volume id.
 */
export function testMultipleAllowedDocumentProviders() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  allowedVolumes = [
    {
      type: VolumeType.DOCUMENTS_PROVIDER,
      id: 'provider_a',
    },
    {type: VolumeType.DOCUMENTS_PROVIDER, id: 'provider_b'},
  ];

  assertTrue(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test multiple identical volume types, with none matching the current volume.
 */
export function testMultipleNoAllowedDocumentProviders() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  allowedVolumes = [
    {
      type: VolumeType.DOCUMENTS_PROVIDER,
      id: 'provider_b',
    },
    {type: VolumeType.DOCUMENTS_PROVIDER, id: 'provider_c'},
  ];

  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test that a single root type (with no volume info) returns true if the
 * allowed volumes only contains root information.
 */
export function testSingleRootTypeNoVolumeTypeOrId() {
  const currentRootType = RootType.DOWNLOADS;
  allowedVolumes = [{
    root: RootType.DOWNLOADS,
  }];
  assertTrue(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test that multiple root types defined in allowed volumes (with one matching
 * the current type) returns true.
 */
export function testMultipleRootTypesNoVolumeTypeOrId() {
  const currentRootType = RootType.DOWNLOADS;
  allowedVolumes = [
    {
      root: RootType.DOWNLOADS,
    },
    {
      root: RootType.DRIVE,
    },
  ];
  assertTrue(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test multiple defined root types with none matching the allowed volumes
 * returns false.
 */
export function testMultipleDisallowedRootTypesNoVolumeTypeOrId() {
  const currentRootType = RootType.SMB;
  allowedVolumes = [
    {
      root: RootType.DOWNLOADS,
    },
    {
      root: RootType.DRIVE,
    },
  ];
  assertFalse(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test both volume info and root type returns true (for stricter checking of
 * a volume type).)
 */
export function testVolumeTypeAndRootTypeNoId() {
  const currentRootType = RootType.SHARED_DRIVE;
  const currentVolume = createAndSetVolumeInfo(VolumeType.DRIVE);
  allowedVolumes = [{
    type: VolumeType.DRIVE,
    root: RootType.SHARED_DRIVE,
  }];
  assertTrue(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test that if volume type matches with but not root type should return false.
 */
export function testVolumeTypeMatchesButNotRootType() {
  const currentRootType = RootType.SHARED_DRIVE;
  const currentVolume = createAndSetVolumeInfo(VolumeType.DRIVE);
  allowedVolumes = [{
    type: VolumeType.DRIVE,
    root: RootType.DRIVE_SHARED_WITH_ME,
  }];
  assertFalse(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test that volume type is defined but the root type is null, allowed volumes
 * should allow for stricter checking of a volume type, this should return
 * false.
 */
export function testVolumeTypeDefinedButNotRootType() {
  const currentVolume = createAndSetVolumeInfo(VolumeType.DRIVE);
  allowedVolumes = [{
    type: VolumeType.DRIVE,
    root: RootType.DRIVE_SHARED_WITH_ME,
  }];
  assertFalse(isAllowedVolume(
      currentVolume, /* currentRootType */ null, allowedVolumes));
}

/**
 * Test that root type is defined but not volume type. If the allowed volumes
 * requires both defined, should return false.
 */
export function testRootTypeDefinedButNotVolumeType() {
  const currentRootType = RootType.TRASH;
  allowedVolumes = [{
    type: VolumeType.TRASH,
    root: RootType.TRASH,
  }];
  assertFalse(isAllowedVolume(
      /* currentVolume */ null, currentRootType, allowedVolumes));
}

/**
 * Test that volume type, root type and id are all defined.
 */
export function testVolumeTypeRootTypeAndIdDefined() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  const currentRootType = RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [{
    type: VolumeType.DOCUMENTS_PROVIDER,
    root: RootType.DOCUMENTS_PROVIDER,
    id: 'provider_a',
  }];
  assertTrue(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test that volume type, root type are defined but id does not match.
 */
export function testVolumeTypeRootTypeAndIdDefinedNoMatch() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  const currentRootType = RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [{
    type: VolumeType.DOCUMENTS_PROVIDER,
    root: RootType.DOCUMENTS_PROVIDER,
    id: 'provider_b',
  }];
  assertFalse(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test multiple volume types, root types and ids defined with one matching.
 */
export function testMultipleDifferentIdsSameVolumeTypeAndRootTypeOneMatches() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeType.DOCUMENTS_PROVIDER, 'provider_a');
  const currentRootType = RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [
    {
      type: VolumeType.DOCUMENTS_PROVIDER,
      root: RootType.DOCUMENTS_PROVIDER,
      id: 'provider_b',
    },
    {
      type: VolumeType.DOCUMENTS_PROVIDER,
      root: RootType.DOCUMENTS_PROVIDER,
      id: 'provider_a',
    },
  ];
  assertTrue(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test multiple volume types, root types and ids defined with none matching.
 */
export function testMultipleDifferentIdsSameVolumeTypeAndRootTypeNoneMatches() {
  const currentVolume =
      createAndSetVolumeInfo(VolumeType.DOCUMENTS_PROVIDER, 'provider_c');
  const currentRootType = RootType.DOCUMENTS_PROVIDER;

  allowedVolumes = [
    {
      type: VolumeType.DOCUMENTS_PROVIDER,
      root: RootType.DOCUMENTS_PROVIDER,
      id: 'provider_b',
    },
    {
      type: VolumeType.DOCUMENTS_PROVIDER,
      root: RootType.DOCUMENTS_PROVIDER,
      id: 'provider_a',
    },
  ];
  assertFalse(isAllowedVolume(currentVolume, currentRootType, allowedVolumes));
}

/**
 * Test undefined types return false.
 */
export function testUndefinedThresholdAndSizeStats() {
  const testMinSizeThreshold = {
    type: RootType.DOWNLOADS,
    minSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  };
  const testMinRatioThreshold = {
    type: RootType.DOWNLOADS,
    minSize: 0.1,
  };
  const testSizeStats = {
    totalSize: 20 * 1024 * 1024 * 1024,     // 20 GB
    remainingSize: 1 * 1024 * 1024 * 1024,  // 1 GB
  };
  const testSizeStatsFull = {
    totalSize: 20 * 1024 * 1024 * 1024,  // 20 GB
    remainingSize: 0,                    // full
  };

  assertFalse(
      isBelowThreshold(undefined as unknown as MinDiskThreshold, undefined));
  assertFalse(isBelowThreshold(testMinSizeThreshold, undefined));
  assertFalse(isBelowThreshold(testMinRatioThreshold, undefined));
  assertFalse(isBelowThreshold(
      undefined as unknown as MinDiskThreshold, testSizeStats));
  assertTrue(isBelowThreshold(testMinRatioThreshold, testSizeStatsFull));
}

/**
 * Test that below, equal to and above the minSize threshold returns correct
 * results.
 */
export function testMinSizeReturnsCorrectly() {
  const createMinSizeThreshold = (minSize: number) => ({
    type: RootType.DOWNLOADS,
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
  const createMinRatioThreshold = (minRatio: number) => ({
    type: RootType.DOWNLOADS,
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
