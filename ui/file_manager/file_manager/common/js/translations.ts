// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';

import type {EntryLocation} from '../../background/js/entry_location_impl.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';

import {isOneDrivePlaceholder} from './entry_utils.js';
import {getMediaViewRootTypeFromVolumeId, MediaViewRootType, RootType} from './volume_manager_types.js';


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
    case RootType.DOWNLOADS:
      return volumeInfoLabel;
    case RootType.DRIVE:
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
    case RootType.SHARED_DRIVE:
    case RootType.SHARED_DRIVES_GRAND_ROOT:
      return str('DRIVE_SHARED_DRIVES_LABEL');
    case RootType.COMPUTER:
    case RootType.COMPUTERS_GRAND_ROOT:
      return str('DRIVE_COMPUTERS_LABEL');
    case RootType.DRIVE_OFFLINE:
      return str('DRIVE_OFFLINE_COLLECTION_LABEL');
    case RootType.DRIVE_SHARED_WITH_ME:
      return str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL');
    case RootType.DRIVE_RECENT:
      return str('DRIVE_RECENT_COLLECTION_LABEL');
    case RootType.DRIVE_FAKE_ROOT:
      return str('DRIVE_DIRECTORY_LABEL');
    case RootType.RECENT:
      return str('RECENT_ROOT_LABEL');
    case RootType.CROSTINI:
      return str('LINUX_FILES_ROOT_LABEL');
    case RootType.MY_FILES:
      return str('MY_FILES_ROOT_LABEL');
    case RootType.TRASH:
      return str('TRASH_ROOT_LABEL');
    case RootType.MEDIA_VIEW:
      const mediaViewRootType = getMediaViewRootTypeFromVolumeId(
          locationInfo.volumeInfo?.volumeId || '');
      switch (mediaViewRootType) {
        case MediaViewRootType.IMAGES:
          return str('MEDIA_VIEW_IMAGES_ROOT_LABEL');
        case MediaViewRootType.VIDEOS:
          return str('MEDIA_VIEW_VIDEOS_ROOT_LABEL');
        case MediaViewRootType.AUDIO:
          return str('MEDIA_VIEW_AUDIO_ROOT_LABEL');
        case MediaViewRootType.DOCUMENTS:
          return str('MEDIA_VIEW_DOCUMENTS_ROOT_LABEL');
        default:
          console.error(
              'Unsupported media view root type: ' + mediaViewRootType);
          return volumeInfoLabel;
      }

    case RootType.ARCHIVE:
    case RootType.REMOVABLE:
    case RootType.MTP:
    case RootType.PROVIDED:
    case RootType.ANDROID_FILES:
    case RootType.DOCUMENTS_PROVIDER:
    case RootType.SMB:
    case RootType.GUEST_OS:
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
  if (isOneDrivePlaceholder(entry)) {
    // Placeholders have locationInfo, but no locationInfo.volumeInfo
    // so getRootTypeLabel() would return null.
    return entry.name;
  }

  if (locationInfo) {
    if (locationInfo.hasFixedLabel) {
      return getRootTypeLabel(locationInfo);
    }

    if (entry.filesystem && entry.filesystem.root === entry) {
      return getRootTypeLabel(locationInfo);
    }
  }

  // Special case for MyFiles/Downloads, MyFiles/PvmDefault and MyFiles/Camera.
  if (locationInfo && locationInfo.rootType === RootType.DOWNLOADS) {
    if (entry.fullPath === '/Downloads') {
      return str('DOWNLOADS_DIRECTORY_LABEL');
    }
    if (entry.fullPath === '/PvmDefault') {
      return str('PLUGIN_VM_DIRECTORY_LABEL');
    }
    if (entry.fullPath === '/Camera') {
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
  if (hours === 0) {
    // Less than one hour. Display remaining time in minutes.
    return strf('TIME_REMAINING_ESTIMATE', minuteFormatter.format(minutes));
  }

  minutes -= hours * 60;

  const hourFormatter = new Intl.NumberFormat(
      locale, {style: 'unit', unit: 'hour', unitDisplay: 'long'});

  if (minutes === 0) {
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

/**
 * Get the plural string with a specified count.
 * Note: the string id to get must be handled by `PluralStringHandler` in C++
 * side.
 *
 * @param id The translation string resource id.
 * @param count The number count to get the plural.
 */
export async function getPluralString(
    id: string, count: number): Promise<string> {
  return PluralStringProxyImpl.getInstance().getPluralString(id, count);
}

/**
 * Get the plural string with a specified count and placeholder values.
 * Note: the string id to get must be handled by `PluralStringHandler` in C++
 * side.
 *
 * ```
 * {NUM_FILE, plural,
 *    = 1 {1 file with <ph name="FILE_SIZE">$1<ex>44 MB</ex></ph> size.},
 *    other {# files with <ph name="FILE_SIZE">$1<ex>44 MB</ex></ph> size.}}
 *
 * await getPluralStringWithPlaceHolders(id, 2, '44 MB')
 * => "2 files with 44 MB size"
 * ```
 *
 * @param id The translation string resource id.
 * @param count The number count to get the plural.
 * @param placeholders The placeholder value to replace.
 */
export async function getPluralStringWithPlaceHolders(
    id: string, count: number,
    ...placeholders: Array<string|number>): Promise<string> {
  const strWithPlaceholders =
      await PluralStringProxyImpl.getInstance().getPluralString(id, count);
  return loadTimeData.substituteString(strWithPlaceholders, ...placeholders);
}
