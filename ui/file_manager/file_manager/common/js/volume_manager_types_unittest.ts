// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockFileEntry, MockFileSystem} from './mock_entry.js';
import {getRootTypeFromVolumeType, getVolumeTypeFromRootType, isRecentArcEntry, RootType, RootTypesForUMA, VolumeType} from './volume_manager_types.js';

// Test that every volumeType has a rootType, and that it maps back to the same
// volumeType.
export function testRootTypeFromVolumeTypeBijection() {
  for (const volumeType of Object.values(VolumeType)) {
    // System Internal volumes do not have a corresponding root.
    if (volumeType === VolumeType.SYSTEM_INTERNAL ||
        volumeType === VolumeType.TESTING) {
      return;
    }

    const rootType = getRootTypeFromVolumeType(volumeType);
    assertEquals(volumeType, getVolumeTypeFromRootType(rootType));
  }
}

// Test that all rootType have a corresponding volumeType, except for "fake"
// root types that do not have a volume of their own.
export function testEveryRootTypeHasAVolumeType() {
  for (const rootType of Object.values(RootType)) {
    // The "Recent" view and "Google Drive" parent entry are not handled in the
    // switch because they do not have a corresponding volume.
    // TODO(tapted): Validate this against isFakeEntry(..) when
    // files_app_entry_types is moved to file_manager/base.
    if (rootType.startsWith('DEPRECATED_') || rootType === RootType.RECENT ||
        rootType === RootType.DRIVE_FAKE_ROOT) {
      return;
    }

    const volumeType = getVolumeTypeFromRootType(rootType);
    assertNotEquals(volumeType, undefined);
  }
}

// Tests that IsRecentArcEntry() should return true/false if an entry belongs/
// doesn't belong to recent.
export function testIsRecentArcEntry() {
  assertFalse(isRecentArcEntry(null));
  const otherEntry = MockFileEntry.create(
      new MockFileSystem('download:Downloads'), 'test.txt');
  assertFalse(isRecentArcEntry(otherEntry));
  const recentEntry = MockFileEntry.create(
      new MockFileSystem(
          'com.android.providers.media.documents:documents_root'),
      'Documents/abc.pdf');
  assertTrue(isRecentArcEntry(recentEntry));
}

// Deprecated roots shouldn't have an enum on RootType, however all te indexes
// in the UMA array has to still match the enums.xml.
export function testRootTypeEnumIndexes() {
  const numDeprecatedRoots =
      RootTypesForUMA.filter(r => r.toLowerCase().startsWith('deprecated'))
          .length;
  assertEquals(
      Object.keys(RootType).length, RootTypesForUMA.length - numDeprecatedRoots,
      'Members in RootTypesForUMA do not match them in RootTypes.');
}
