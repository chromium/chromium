// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

/**
 * Whether the Files app integration with DLP (Data Loss Prevention) is enabled.
 */
export function isDlpEnabled() {
  return loadTimeData.valueExists('DLP_ENABLED') &&
      loadTimeData.getBoolean('DLP_ENABLED');
}

/**
 * Returns true if FuseBoxDebug flag is enabled.
 */
export function isFuseBoxDebugEnabled() {
  return loadTimeData.isInitialized() &&
      loadTimeData.valueExists('FUSEBOX_DEBUG') &&
      loadTimeData.getBoolean('FUSEBOX_DEBUG');
}

/**
 * Returns true if GuestOsFiles flag is enabled.
 */
export function isGuestOsEnabled() {
  return loadTimeData.getBoolean('GUEST_OS');
}

/**
 * Returns true if Jelly flag is enabled.
 */
export function isJellyEnabled() {
  return loadTimeData.getBoolean('JELLY');
}

/**
 * Returns true if the cros-components flag is enabled.
 */
export function isCrosComponentsEnabled() {
  return loadTimeData.getBoolean('CROS_COMPONENTS');
}

/**
 * Returns true if DriveFsMirroring flag is enabled.
 */
export function isMirrorSyncEnabled() {
  return loadTimeData.isInitialized() &&
      loadTimeData.valueExists('DRIVEFS_MIRRORING') &&
      loadTimeData.getBoolean('DRIVEFS_MIRRORING');
}

export function isGoogleOneOfferFilesBannerEligibleAndEnabled() {
  return loadTimeData.getBoolean(
      'ELIGIBLE_AND_ENABLED_GOOGLE_ONE_OFFER_FILES_BANNER');
}

/**
 * Returns true if FilesSinglePartitionFormat flag is enabled.
 */
export function isSinglePartitionFormatEnabled() {
  return loadTimeData.getBoolean('FILES_SINGLE_PARTITION_FORMAT_ENABLED');
}

/**
 * Returns true if InlineSyncStatus feature flag is enabled.
 */
export function isInlineSyncStatusEnabled() {
  return loadTimeData.valueExists('INLINE_SYNC_STATUS') &&
      loadTimeData.getBoolean('INLINE_SYNC_STATUS');
}

/**
 * Returns true if FilesDriveShortcuts flag is enabled.
 */
export function isDriveShortcutsEnabled() {
  return loadTimeData.isInitialized() &&
      loadTimeData.valueExists('DRIVE_SHORTCUTS') &&
      loadTimeData.getBoolean('DRIVE_SHORTCUTS');
}

/**
 * Returns whether the DriveFsBulkPinning feature flag is enabled.
 */
export function isDriveFsBulkPinningEnabled() {
  return loadTimeData.getBoolean('DRIVE_FS_BULK_PINNING');
}

/**
 * Whether the new directory tree flag is enabled.
 */
export function isNewDirectoryTreeEnabled() {
  return loadTimeData.valueExists('NEW_DIRECTORY_TREE') &&
      loadTimeData.getBoolean('NEW_DIRECTORY_TREE');
}

export function isArcUsbStorageUIEnabled() {
  return loadTimeData.valueExists('ARC_USB_STORAGE_UI_ENABLED') &&
      loadTimeData.getBoolean('ARC_USB_STORAGE_UI_ENABLED');
}

export function isArcVmEnabled() {
  return loadTimeData.valueExists('ARC_VM_ENABLED') &&
      loadTimeData.getBoolean('ARC_VM_ENABLED');
}

export function isPluginVmEnabled() {
  return loadTimeData.valueExists('PLUGIN_VM_ENABLED') &&
      loadTimeData.getBoolean('PLUGIN_VM_ENABLED');
}
