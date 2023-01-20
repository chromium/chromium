// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManagerCommon} from '../common/js/volume_manager_types.js';

/**
 * All Banners are extended from this interface.
 * @abstract
 */
export class Banner extends HTMLElement {
  /**
   * Returns the volume types or roots where the banner is enabled.
   * @return {!Array<!Banner.AllowedVolume>}
   * @abstract
   */
  allowedVolumes() {}

  /**
   * The number of Files app sessions a banner can be shown for. A session is
   * defined as a new window for Files app. If a user opens a window, the same
   * session is maintained until either that window is closed or another window
   * is opened.
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
   * @return {!Banner.DiskThresholdMinSize|!Banner.DiskThresholdMinRatio|undefined}
   */
  diskThreshold() {}

  /**
   * The duration (in seconds) to hide the banner after it has been dismissed by
   * the user.
   * @return {number|undefined}
   */
  hideAfterDismissedDurationSeconds() {}

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

  /**
   * When a custom filter is registered for a banner and the banner is shown,
   * some context can be passed back to the banner to update.
   * @param {!Object} context Custom context passed to banner when shown.
   */
  onFilteredContext(context) {}
}

/**
 * DocumentProvider's all show as ANDROID_FILES, use the id field to match on
 * the exact DocumentProvider.
 * @typedef {{
 *            type: VolumeManagerCommon.VolumeType,
 *            root: (VolumeManagerCommon.RootType|null),
 *            id: (string|null),
 *          }}
 */
Banner.AllowedVolumeType;

/**
 * An explicitly defined volume root type with an optional volume type and id.
 * Main use for "fake" volumes such as Trash that don't report a volumeType.
 * @typedef {{
 *            root: VolumeManagerCommon.RootType,
 *            type: (VolumeManagerCommon.VolumeType|null),
 *            id: (string|null),
 *          }}
 */
Banner.AllowedRootType;

/**
 * @typedef {!Banner.AllowedVolumeType|!Banner.AllowedRootType}
 */
Banner.AllowedVolume;

/**
 * The minSize is denoted in bytes.
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

/**
 * Events dispatched by concrete banners.
 * @enum {string}
 * @const
 */
Banner.Event = {
  BANNER_DISMISSED: 'banner-dismissed',
  BANNER_DISMISSED_FOREVER: 'banner-dismissed-forever',
};

/**
 * Event source for BANNER_DISMISSED_FOREVER event.
 * @enum {string}
 * @const
 */
Banner.DismissedForeverEventSource = {
  EXTRA_BUTTON: 'extra-button',
  DEFAULT_DISMISS_BUTTON: 'default-dismiss-button',
  OVERRIDEN_DISMISS_BUTTON: 'overriden-dismiss-button',
};

/**
 * Used to denote a banner that shows has an infinite time limit.
 * @const {number}
 */
Banner.INIFINITE_TIME = 0;

/**
 * @typedef {{
 *            shouldShow: !(function(): boolean),
 *            context: !function(),
 *          }}
 */
Banner.CustomFilter;
