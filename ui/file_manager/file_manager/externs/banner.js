// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManagerCommon} from '../common/js/volume_manager_types.js';

/**
 * All Banners are extended from this interface.
 * @abstract
 */
export class Banner extends HTMLElement {
  /**
   * Returns the volume types the banner where the banner is enabled.
   * @return {!Array<!Banner.AllowedVolumeType>}
   * @abstract
   */
  allowedVolumeTypes() {}

  /**
   * The number of Files app sessions a banner can be shown for.
   * @return {number|undefined}
   */
  showLimit() {}

  /**
   * The seconds that a banner is to remain visible.
   * @return {number|undefined}
   */
  timeLimit() {}

  /**
   * The size threshold to trigger a banner if it goes below.
   * @return {!Banner.DiskThresholdMinSize|!Banner.DiskThresholdMinRatio|!undefined}
   */
  diskThreshold() {}

  /**
   * Volume types to watch if they unmount at all.
   * @return {!Array<!VolumeManagerCommon.VolumeType|!undefined>}
   */
  unmountedVolumeTypes() {}

  /**
   * Drive connection state to trigger the banner on.
   * @return {!chrome.fileManagerPrivate.DriveConnectionState|!undefined}
   */
  driveConnectionState() {}

  /**
   * Lifecycle method used to notify Banner implementations when they've been
   * shown.
   */
  onShow() {}
}

/**
 * DocumentProvider's all show as ANDROID_FILES, use the id field to match on
 * the exact DocumentProvider.
 * @typedef {{
 *            type: VolumeManagerCommon.VolumeType,
 *            id: (string|null),
 *          }}
 */
Banner.AllowedVolumeType;

/**
 * @typedef {{
 *            type: VolumeManagerCommon.VolumeType,
 *            minSize: number,
 *          }}
 */
Banner.DiskThresholdMinSize;

/**
 * @typedef {{
 *            type: VolumeManagerCommon.VolumeType,
 *            minRatio: number,
 *          }}
 */
Banner.DiskThresholdMinRatio;
