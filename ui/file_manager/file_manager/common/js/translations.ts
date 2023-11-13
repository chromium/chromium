// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import type {EntryLocation} from '../../externs/entry_location.js';
import type {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

import {VolumeManagerCommon} from './volume_manager_types.js';

/**
 * Returns a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getString(id).
 */
export function str(id: string) {
  try {
    return loadTimeData.getString(id);
  } catch (e) {
    console.warn('Failed to get string for', id);
    return id;
  }
}

/**
 * Returns a translated string with arguments replaced.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getStringF(id, ...).
 */
export function strf(id: string, ...args: any[]) {
  return loadTimeData.getStringF.apply(loadTimeData, [id, ...args]);
}

/**
 * Collator for sorting.
 */
export const collator =
    new Intl.Collator([], {usage: 'sort', numeric: true, sensitivity: 'base'});


/**
 * Returns normalized current locale, or default locale - 'en'.
 */
export function getCurrentLocaleOrDefault() {
  const locale = str('UI_LOCALE') || 'en';
  return locale.replace(/_/g, '-');
}

/**
 * Convert a number of bytes into a human friendly format, using the correct
 * number separators.
 */
export function bytesToString(bytes: number, addedPrecision: number = 0) {
  // Translation identifiers for size units.
  const UNITS = [
    'SIZE_BYTES',
    'SIZE_KB',
    'SIZE_MB',
    'SIZE_GB',
    'SIZE_TB',
    'SIZE_PB',
  ];

  // Minimum values for the units above.
  const STEPS = [
    0,
    Math.pow(2, 10),
    Math.pow(2, 20),
    Math.pow(2, 30),
    Math.pow(2, 40),
    Math.pow(2, 50),
  ];

  // Rounding with precision.
  const round = (value: number, decimals: number) => {
    const scale = Math.pow(10, decimals);
    return Math.round(value * scale) / scale;
  };

  const str = (n: number, u: string) => {
    return strf(u, n.toLocaleString());
  };

  const fmt = (s: number, u: string) => {
    const rounded = round(bytes / s, 1 + addedPrecision);
    return str(rounded, u);
  };

  // Less than 1KB is displayed like '80 bytes'.
  if (bytes < STEPS[1]!) {
    return str(bytes, UNITS[0]!);
  }

  // Up to 1MB is displayed as rounded up number of KBs, or with the desired
  // number of precision digits.
  if (bytes < STEPS[2]!) {
    const rounded = addedPrecision ? round(bytes / STEPS[1]!, addedPrecision) :
                                     Math.ceil(bytes / STEPS[1]!);
    return str(rounded, UNITS[1]!);
  }

  // This loop index is used outside the loop if it turns out |bytes|
  // requires the largest unit.
  let i;

  for (i = 2 /* MB */; i < UNITS.length - 1; i++) {
    if (bytes < STEPS[i + 1]!) {
      return fmt(STEPS[i]!, UNITS[i]!);
    }
  }

  return fmt(STEPS[i]!, UNITS[i]!);
}


/**
 * Returns the localized name of the root type.
 */
export function getRootTypeLabel(locationInfo: EntryLocation) {
  const volumeInfoLabel = locationInfo.volumeInfo?.label || '';
  switch (locationInfo.rootType) {
    case VolumeManagerCommon.RootType.DOWNLOADS:
      return volumeInfoLabel;
    case VolumeManagerCommon.RootType.DRIVE:
      return str('DRIVE_MY_DRIVE_LABEL');
    // |locationInfo| points to either the root directory of an individual Team
    // Drive or sub-directory under it, but not the Shared Drives grand
    // directory. Every Shared Drive and its sub-directories always have
    // individual names (locationInfo.hasFixedLabel is false). So
    // getRootTypeLabel() is used by PathComponent.computeComponentsFromEntry()
    // to display the ancestor name in the breadcrumb like this:
    //   Shared Drives > ABC Shared Drive > Folder1
    //   ^^^^^^^^^^^
    // By this reason, we return the label of the Shared Drives grand root here.
    case VolumeManagerCommon.RootType.SHARED_DRIVE:
    case VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT:
      return str('DRIVE_SHARED_DRIVES_LABEL');
    case VolumeManagerCommon.RootType.COMPUTER:
    case VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT:
      return str('DRIVE_COMPUTERS_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_OFFLINE:
      return str('DRIVE_OFFLINE_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME:
      return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_RECENT:
      return str('DRIVE_RECENT_COLLECTION_LABEL');
    case VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT:
      return str('DRIVE_DIRECTORY_LABEL');
    case VolumeManagerCommon.RootType.RECENT:
      return str('RECENT_ROOT_LABEL');
    case VolumeManagerCommon.RootType.CROSTINI:
      return str('LINUX_FILES_ROOT_LABEL');
    case VolumeManagerCommon.RootType.MY_FILES:
      return str('MY_FILES_ROOT_LABEL');
    case VolumeManagerCommon.RootType.TRASH:
      return str('TRASH_ROOT_LABEL');
    case VolumeManagerCommon.RootType.MEDIA_VIEW:
      const mediaViewRootType =
          VolumeManagerCommon.getMediaViewRootTypeFromVolumeId(
              locationInfo.volumeInfo?.volumeId || '');
      switch (mediaViewRootType) {
        case VolumeManagerCommon.MediaViewRootType.IMAGES:
          return str('MEDIA_VIEW_IMAGES_ROOT_LABEL');
        case VolumeManagerCommon.MediaViewRootType.VIDEOS:
          return str('MEDIA_VIEW_VIDEOS_ROOT_LABEL');
        case VolumeManagerCommon.MediaViewRootType.AUDIO:
          return str('MEDIA_VIEW_AUDIO_ROOT_LABEL');
        case VolumeManagerCommon.MediaViewRootType.DOCUMENTS:
          return str('MEDIA_VIEW_DOCUMENTS_ROOT_LABEL');
      }
      console.error('Unsupported media view root type: ' + mediaViewRootType);
      return volumeInfoLabel;
    case VolumeManagerCommon.RootType.ARCHIVE:
    case VolumeManagerCommon.RootType.REMOVABLE:
    case VolumeManagerCommon.RootType.MTP:
    case VolumeManagerCommon.RootType.PROVIDED:
    case VolumeManagerCommon.RootType.ANDROID_FILES:
    case VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER:
    case VolumeManagerCommon.RootType.SMB:
    case VolumeManagerCommon.RootType.GUEST_OS:
      return volumeInfoLabel;
    default:
      console.error('Unsupported root type: ' + locationInfo.rootType);
      return volumeInfoLabel;
  }
}

/**
 * Returns the localized/i18n name of the entry.
 */
export function getEntryLabel(
    locationInfo: EntryLocation|null, entry: Entry|FilesAppEntry) {
  if (locationInfo) {
    if (locationInfo.hasFixedLabel) {
      return getRootTypeLabel(locationInfo);
    }

    if (entry.filesystem && entry.filesystem.root === entry) {
      return getRootTypeLabel(locationInfo);
    }
  }

  // Special case for MyFiles/Downloads, MyFiles/PvmDefault and MyFiles/Camera.
  if (locationInfo &&
      locationInfo.rootType == VolumeManagerCommon.RootType.DOWNLOADS) {
    if (entry.fullPath == '/Downloads') {
      return str('DOWNLOADS_DIRECTORY_LABEL');
    }
    if (entry.fullPath == '/PvmDefault') {
      return str('PLUGIN_VM_DIRECTORY_LABEL');
    }
    if (entry.fullPath == '/Camera') {
      return str('CAMERA_DIRECTORY_LABEL');
    }
  }

  return entry.name;
}

/**
 * Get the locale based week start from the load time data.
 */
export function getLocaleBasedWeekStart() {
  return loadTimeData.valueExists('WEEK_START_FROM') ?
      loadTimeData.getInteger('WEEK_START_FROM') :
      0;
}

/**
 * Converts seconds into a time remaining string.
 */
export function secondsToRemainingTimeString(seconds: number) {
  const locale = getCurrentLocaleOrDefault();
  let minutes = Math.ceil(seconds / 60);
  if (minutes <= 1) {
    // Less than one minute. Display remaining time in seconds.
    const formatter = new Intl.NumberFormat(
        locale, {style: 'unit', unit: 'second', unitDisplay: 'long'});
    return strf(
        'TIME_REMAINING_ESTIMATE', formatter.format(Math.ceil(seconds)));
  }

  const minuteFormatter = new Intl.NumberFormat(
      locale, {style: 'unit', unit: 'minute', unitDisplay: 'long'});

  const hours = Math.floor(minutes / 60);
  if (hours == 0) {
    // Less than one hour. Display remaining time in minutes.
    return strf('TIME_REMAINING_ESTIMATE', minuteFormatter.format(minutes));
  }

  minutes -= hours * 60;

  const hourFormatter = new Intl.NumberFormat(
      locale, {style: 'unit', unit: 'hour', unitDisplay: 'long'});

  if (minutes == 0) {
    // Hours but no minutes.
    return strf('TIME_REMAINING_ESTIMATE', hourFormatter.format(hours));
  }

  // Hours and minutes.
  return strf(
      'TIME_REMAINING_ESTIMATE_2', hourFormatter.format(hours),
      minuteFormatter.format(minutes));
}

/**
 * Mapping table of file error name to i18n localized error name.
 */
const FileErrorLocalizedName: Record<string, string> = {
  'InvalidModificationError': 'FILE_ERROR_INVALID_MODIFICATION',
  'InvalidStateError': 'FILE_ERROR_INVALID_STATE',
  'NoModificationAllowedError': 'FILE_ERROR_NO_MODIFICATION_ALLOWED',
  'NotFoundError': 'FILE_ERROR_NOT_FOUND',
  'NotReadableError': 'FILE_ERROR_NOT_READABLE',
  'PathExistsError': 'FILE_ERROR_PATH_EXISTS',
  'QuotaExceededError': 'FILE_ERROR_QUOTA_EXCEEDED',
  'SecurityError': 'FILE_ERROR_SECURITY',
};

/**
 * Returns i18n localized error name for file error |name|.
 */
export function getFileErrorString(name: string|null|undefined) {
  const error = name && name in FileErrorLocalizedName ?
      FileErrorLocalizedName[name] :
      'FILE_ERROR_GENERIC';
  return loadTimeData.getString(error!);
}
