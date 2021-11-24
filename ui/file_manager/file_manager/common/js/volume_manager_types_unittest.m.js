// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://test/chai_assert.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

// Test that every volumeType has a rootType, and that it maps back to the same
// volumeType.
export function testRootTypeFromVolumeTypeBijection() {
  Object.keys(VolumeManagerCommon.VolumeType).forEach((key) => {
    const volumeType = VolumeManagerCommon.VolumeType[key];
    assertTrue(volumeType !== undefined);

    // The enum is decorated with an isNative() helper. Skip it for the purposes
    // of this test, since it is not a valid enum value. (This helper breaks the
    // ability to iterate over enum values, so should probably be removed).
    if (volumeType === VolumeManagerCommon.VolumeType.isNative) {
      return;
    }

    // System Internal volumes do not have a corresponding root.
    if (volumeType == VolumeManagerCommon.VolumeType.SYSTEM_INTERNAL) {
      return;
    }

    const rootType = VolumeManagerCommon.getRootTypeFromVolumeType(volumeType);
    assertTrue(
        volumeType == VolumeManagerCommon.getVolumeTypeFromRootType(rootType));
  });
}

// Test that all rootType have a corresponding volumeType, except for "fake"
// root types that do not have a volume of their own.
export function testEveryRootTypeHasAVolumeType() {
  Object.keys(VolumeManagerCommon.RootType).forEach((key) => {
    const rootType = VolumeManagerCommon.RootType[key];
    assertTrue(rootType !== undefined);

    // The "Recent" view and "Google Drive" parent entry are not handled in the
    // switch because they do not have a corresponding volume.
    // TODO(tapted): Validate this against util.isFakeEntry(..) when
    // files_app_entry_types is moved to file_manager/base.
    if (rootType === VolumeManagerCommon.RootType.RECENT ||
        rootType === VolumeManagerCommon.RootType.RECENT_AUDIO ||
        rootType === VolumeManagerCommon.RootType.RECENT_IMAGES ||
        rootType === VolumeManagerCommon.RootType.RECENT_VIDEOS ||
        rootType === VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT ||
        rootType ===
            VolumeManagerCommon.RootType.DEPRECATED_ADD_NEW_SERVICES_MENU) {
      return;
    }

    const volumeType = VolumeManagerCommon.getVolumeTypeFromRootType(rootType);
    assertTrue(volumeType !== undefined);
  });
}
