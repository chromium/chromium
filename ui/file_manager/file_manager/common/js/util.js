// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain utility functions used only by the
 * files app. Other shared utility functions can be found in base/*_util.js,
 * which allows finer-grained control over introducing dependencies.
 */

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {EntryLocation} from '../../externs/entry_location.js';
import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

import {promisify} from './api.js';
import {isDriveFsBulkPinningEnabled} from './flags.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

/**
 * Namespace for utility functions.
 */
const util = {};

/**
 * @param {!chrome.fileManagerPrivate.IconSet} iconSet Set of icons.
 * @return {string} CSS value.
 */
util.iconSetToCSSBackgroundImageValue = iconSet => {
  let lowDpiPart = null;
  let highDpiPart = null;
  if (iconSet.icon16x16Url) {
    lowDpiPart = 'url(' + iconSet.icon16x16Url + ') 1x';
  }
  if (iconSet.icon32x32Url) {
    highDpiPart = 'url(' + iconSet.icon32x32Url + ') 2x';
  }

  if (lowDpiPart && highDpiPart) {
    return '-webkit-image-set(' + lowDpiPart + ', ' + highDpiPart + ')';
  } else if (lowDpiPart) {
    return '-webkit-image-set(' + lowDpiPart + ')';
  } else if (highDpiPart) {
    return '-webkit-image-set(' + highDpiPart + ')';
  }

  return 'none';
};

/**
 * Mapping table of file error name to i18n localized error name.
 *
 * @const @enum {string}
 */
util.FileErrorLocalizedName = {
  'InvalidModificationError': 'FILE_ERROR_INVALID_MODIFICATION',
  'InvalidStateError': 'FILE_ERROR_INVALID_STATE',
  'NoModificationAllowedError': 'FILE_ERROR_NO_MODIFICATION_ALLOWED',
  'NotFoundError': 'FILE_ERROR_NOT_FOUND',
  'NotReadableError': 'FILE_ERROR_NOT_READABLE',
  'PathExistsError': 'FILE_ERROR_PATH_EXISTS',
  'QuotaExceededError': 'FILE_ERROR_QUOTA_EXCEEDED',
  'SecurityError': 'FILE_ERROR_SECURITY',
};
Object.freeze(util.FileErrorLocalizedName);

/**
 * Returns i18n localized error name for file error |name|.
 *
 * @param {?string|undefined} name File error name.
 * @return {string} Translated file error string.
 */
util.getFileErrorString = name => {
  // @ts-ignore: error TS2538: Type 'undefined' cannot be used as an index type.
  const error = util.FileErrorLocalizedName[name] || 'FILE_ERROR_GENERIC';
  return loadTimeData.getString(error);
};

/**
 * Mapping table for FileError.code style enum to DOMError.name string.
 *
 * @const @enum {string}
 */
util.FileError = {
  ABORT_ERR: 'AbortError',
  INVALID_MODIFICATION_ERR: 'InvalidModificationError',
  INVALID_STATE_ERR: 'InvalidStateError',
  NO_MODIFICATION_ALLOWED_ERR: 'NoModificationAllowedError',
  NOT_FOUND_ERR: 'NotFoundError',
  NOT_READABLE_ERR: 'NotReadable',
  PATH_EXISTS_ERR: 'PathExistsError',
  QUOTA_EXCEEDED_ERR: 'QuotaExceededError',
  TYPE_MISMATCH_ERR: 'TypeMismatchError',
  ENCODING_ERR: 'EncodingError',
};
Object.freeze(util.FileError);

/**
 * Convert a number of bytes into a human friendly format, using the correct
 * number separators.
 *
 * @param {number} bytes The number of bytes.
 * @param {number=} addedPrecision The number of precision digits to add.
 * @return {string} Localized string.
 */
util.bytesToString = (bytes, addedPrecision = 0) => {
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
  // @ts-ignore: error TS7006: Parameter 'decimals' implicitly has an 'any'
  // type.
  const round = (value, decimals) => {
    const scale = Math.pow(10, decimals);
    return Math.round(value * scale) / scale;
  };

  // @ts-ignore: error TS7006: Parameter 'u' implicitly has an 'any' type.
  const str = (n, u) => {
    return strf(u, n.toLocaleString());
  };

  // @ts-ignore: error TS7006: Parameter 'u' implicitly has an 'any' type.
  const fmt = (s, u) => {
    const rounded = round(bytes / s, 1 + addedPrecision);
    return str(rounded, u);
  };

  // Less than 1KB is displayed like '80 bytes'.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  if (bytes < STEPS[1]) {
    return str(bytes, UNITS[0]);
  }

  // Up to 1MB is displayed as rounded up number of KBs, or with the desired
  // number of precision digits.
  // @ts-ignore: error TS2532: Object is possibly 'undefined'.
  if (bytes < STEPS[2]) {
    const rounded = addedPrecision ?
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        round(bytes / STEPS[1], addedPrecision) :
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        Math.ceil(bytes / STEPS[1]);
    return str(rounded, UNITS[1]);
  }

  // This loop index is used outside the loop if it turns out |bytes|
  // requires the largest unit.
  let i;

  for (i = 2 /* MB */; i < UNITS.length - 1; i++) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (bytes < STEPS[i + 1]) {
      return fmt(STEPS[i], UNITS[i]);
    }
  }

  return fmt(STEPS[i], UNITS[i]);
};

/**
 * Extracts path from filesystem: URL.
 * @param {?string=} url Filesystem URL.
 * @return {?string} The path if it can be parsed, null if it cannot.
 */
util.extractFilePath = url => {
  const match =
      /^filesystem:[\w-]*:\/\/[\w-]*\/(external|persistent|temporary)(\/.*)$/
          .exec(url || '');
  const path = match && match[2];
  if (!path) {
    return null;
  }
  return decodeURIComponent(path);
};

/**
 * Returns a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 * Equivalent to loadTimeData.getString(id).
 *
 * @param {string} id The id of the string to return.
 * @return {string} The translated string.
 */
export function str(id) {
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
 *
 * @param {string} id The id of the string to return.
 * @param {...*} var_args The values to replace into the string.
 * @return {string} The translated string with replaced values.
 */
// @ts-ignore: error TS6133: 'var_args' is declared but its value is never read.
export function strf(id, var_args) {
  // @ts-ignore: error TS2345: Argument of type 'IArguments' is not assignable
  // to parameter of type '[id: string, ...args: (string | number)[]]'.
  return loadTimeData.getStringF.apply(loadTimeData, arguments);
}

// Export strf() into the util namespace.
util.strf = strf;

/**
 * @return {boolean} True if the Files app is running as an open files or a
 *     select folder dialog. False otherwise.
 */
util.runningInBrowser = () => {
  // @ts-ignore: error TS2339: Property 'appID' does not exist on type 'Window &
  // typeof globalThis'.
  return !window.appID;
};

/**
 * The type of a file operation.
 * @enum {string}
 * @const
 */
util.FileOperationType = {
  COPY: 'COPY',
  DELETE: 'DELETE',
  MOVE: 'MOVE',
  RESTORE: 'RESTORE',
  RESTORE_TO_DESTINATION: 'RESTORE_TO_DESTINATION',
  ZIP: 'ZIP',
};
Object.freeze(util.FileOperationType);

/**
 * The type of a file operation error.
 * @enum {number}
 * @const
 */
util.FileOperationErrorType = {
  UNEXPECTED_SOURCE_FILE: 0,
  TARGET_EXISTS: 1,
  FILESYSTEM_ERROR: 2,
};
Object.freeze(util.FileOperationErrorType);

/**
 * Collator for sorting.
 * @type {Intl.Collator}
 */
util.collator =
    new Intl.Collator([], {usage: 'sort', numeric: true, sensitivity: 'base'});

/**
 * The last URL with visitURL().
 * @private @type {string}
 */
// @ts-ignore: error TS7034: Variable 'lastVisitedURL' implicitly has type 'any'
// in some locations where its type cannot be determined.
let lastVisitedURL;

/**
 * Visit the URL.
 *
 * If the browser is opening, the url is opened in a new tab, otherwise the url
 * is opened in a new window.
 *
 * @param {!string} url URL to visit.
 */
util.visitURL = url => {
  lastVisitedURL = url;
  // openURL opens URLs in the primary browser (ash vs lacros) as opposed to
  // window.open which always opens URLs in ash-chrome.
  chrome.fileManagerPrivate.openURL(url);
};

/**
 * Return the last URL visited with visitURL().
 *
 * @return {string} The last URL visited.
 */
util.getLastVisitedURL = () => {
  // @ts-ignore: error TS7005: Variable 'lastVisitedURL' implicitly has an 'any'
  // type.
  return lastVisitedURL;
};

/**
 * Returns normalized current locale, or default locale - 'en'.
 * @return {string} Current locale
 */
util.getCurrentLocaleOrDefault = () => {
  const locale = str('UI_LOCALE') || 'en';
  return locale.replace(/_/g, '-');
};

/**
 * Returns whether the window is teleported or not.
 * @param {Window} window Window.
 * @return {Promise<boolean>} Whether the window is teleported or not.
 */
util.isTeleported = window => {
  return new Promise(onFulfilled => {
    // @ts-ignore: error TS2339: Property 'chrome' does not exist on type
    // 'Window'.
    window.chrome.fileManagerPrivate.getProfiles(
        // @ts-ignore: error TS7006: Parameter 'displayedId' implicitly has an
        // 'any' type.
        (profiles, currentId, displayedId) => {
          onFulfilled(currentId !== displayedId);
        });
  });
};

/**
 * Runs chrome.test.sendMessage in test environment. Does nothing if running
 * in production environment.
 *
 * @param {string} message Test message to send.
 */
util.testSendMessage = message => {
  // @ts-ignore: error TS2339: Property 'chrome' does not exist on type
  // 'Window'.
  const test = chrome.test || window.top.chrome.test;
  if (test) {
    test.sendMessage(message);
  }
};

/**
 * Extracts the extension of the path.
 *
 * Examples:
 * util.splitExtension('abc.ext') -> ['abc', '.ext']
 * util.splitExtension('a/b/abc.ext') -> ['a/b/abc', '.ext']
 * util.splitExtension('a/b') -> ['a/b', '']
 * util.splitExtension('.cshrc') -> ['', '.cshrc']
 * util.splitExtension('a/b.backup/hoge') -> ['a/b.backup/hoge', '']
 *
 * @param {string} path Path to be extracted.
 * @return {Array<string>} Filename and extension of the given path.
 */
util.splitExtension = path => {
  let dotPosition = path.lastIndexOf('.');
  if (dotPosition <= path.lastIndexOf('/')) {
    dotPosition = -1;
  }

  const filename = dotPosition != -1 ? path.substr(0, dotPosition) : path;
  const extension = dotPosition != -1 ? path.substr(dotPosition) : '';
  return [filename, extension];
};

/**
 * Returns the localized name of the root type.
 * @param {!EntryLocation} locationInfo Location info.
 * @return {string} The localized name.
 */
util.getRootTypeLabel = locationInfo => {
  switch (locationInfo.rootType) {
    case VolumeManagerCommon.RootType.DOWNLOADS:
      return locationInfo.volumeInfo.label;
    case VolumeManagerCommon.RootType.DRIVE:
      return str('DRIVE_MY_DRIVE_LABEL');
    case VolumeManagerCommon.RootType.SHARED_DRIVE:
    // |locationInfo| points to either the root directory of an individual Team
    // Drive or sub-directory under it, but not the Shared Drives grand
    // directory. Every Shared Drive and its sub-directories always have
    // individual names (locationInfo.hasFixedLabel is false). So
    // getRootTypeLabel() is used by PathComponent.computeComponentsFromEntry()
    // to display the ancestor name in the breadcrumb like this:
    //   Shared Drives > ABC Shared Drive > Folder1
    //   ^^^^^^^^^^^
    // By this reason, we return the label of the Shared Drives grand root here.
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
              locationInfo.volumeInfo.volumeId);
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
      return locationInfo.volumeInfo.label;
    case VolumeManagerCommon.RootType.ARCHIVE:
    case VolumeManagerCommon.RootType.REMOVABLE:
    case VolumeManagerCommon.RootType.MTP:
    case VolumeManagerCommon.RootType.PROVIDED:
    case VolumeManagerCommon.RootType.ANDROID_FILES:
    case VolumeManagerCommon.RootType.DOCUMENTS_PROVIDER:
    case VolumeManagerCommon.RootType.SMB:
    case VolumeManagerCommon.RootType.GUEST_OS:
      return locationInfo.volumeInfo.label;
    default:
      console.error('Unsupported root type: ' + locationInfo.rootType);
      return locationInfo.volumeInfo.label;
  }
};

/**
 * Returns the localized/i18n name of the entry.
 *
 * @param {?EntryLocation} locationInfo
 * @param {!Entry|!FilesAppEntry} entry The entry to be retrieve the name of.
 * @return {string} The localized name.
 */
util.getEntryLabel = (locationInfo, entry) => {
  if (locationInfo) {
    if (locationInfo.hasFixedLabel) {
      return util.getRootTypeLabel(locationInfo);
    }

    if (entry.filesystem && entry.filesystem.root === entry) {
      return util.getRootTypeLabel(locationInfo);
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
};

/**
 * Checks if an API call returned an error, and if yes then prints it.
 */
util.checkAPIError = () => {
  if (chrome.runtime.lastError) {
    console.warn(chrome.runtime.lastError.message);
  }
};

/**
 * Makes a promise which will be fulfilled |ms| milliseconds later.
 * @param {number} ms The delay in milliseconds.
 * @return {!Promise<void>}
 */
// @ts-ignore: error TS2314: Generic type 'Promise<T>' requires 1 type
// argument(s).
util.delay = ms => {
  return new Promise(resolve => {
    setTimeout(resolve, ms);
  });
};

/**
 * Makes a promise which will be rejected if the given |promise| is not resolved
 * or rejected for |ms| milliseconds.
 * @param {!Promise<*>} promise A promise which needs to be timed out.
 * @param {number} ms Delay for the timeout in milliseconds.
 * @param {string=} opt_message Error message for the timeout.
// @ts-ignore: error TS2314: Generic type 'Promise<T>' requires 1 type
argument(s).
 * @return {!Promise<*>} A promise which can be rejected by timeout.
 */
util.timeoutPromise = (promise, ms, opt_message) => {
  return Promise.race([
    // @ts-ignore: error TS2314: Generic type 'Promise<T>' requires 1 type
    // argument(s).
    promise,
    util.delay(ms).then(() => {
      throw new Error(opt_message || 'Operation timed out.');
    }),
  ]);
};

/**
 * Executes a functions only when the context is not the incognito one in a
 * regular session. Returns a promise that when fulfilled informs us whether or
 * not the callback was invoked.
 * @param {function():void} callback
 * @return {!Promise<boolean>}
 */
util.doIfPrimaryContext = async (callback) => {
  const guestMode = await util.isInGuestMode();
  if (guestMode) {
    callback();
    return true;
  }
  return false;
};

/**
 * Returns the Files app modal dialog used to embed any files app dialog
 * that derives from cr.ui.dialogs.
 *
 * @return {!HTMLDialogElement}
 */
util.getFilesAppModalDialogInstance = () => {
  let dialogElement = document.querySelector('#files-app-modal-dialog');

  if (!dialogElement) {  // Lazily create the files app dialog instance.
    dialogElement = document.createElement('dialog');
    dialogElement.id = 'files-app-modal-dialog';
    document.body.appendChild(dialogElement);
  }

  return /** @type {!HTMLDialogElement} */ (dialogElement);
};

/**
 *
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} left
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} right
 * @returns {boolean}
 */
util.descriptorEqual = function(left, right) {
  return left.appId === right.appId && left.taskType === right.taskType &&
      left.actionId === right.actionId;
};

/**
 * Create a taskID which is a string unique-ID for a task. This is temporary
 * and will be removed once we use task.descriptor everywhere instead.
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor
 * @returns {string}
 */
util.makeTaskID = function({appId, taskType, actionId}) {
  return `${appId}|${taskType}|${actionId}`;
};

/**
 * Returns a new promise which, when fulfilled carries a boolean indicating
 * whether the app is in the guest mode. Typical use:
 *
 * util.isInGuestMode().then(
 *     (guest) => { if (guest) { ... in guest mode } }
 * );
 * @return {Promise<boolean>}
 */
util.isInGuestMode = async () => {
  const profiles = await promisify(chrome.fileManagerPrivate.getProfiles);
  return profiles.length > 0 && profiles[0].profileId === '$guest';
};

/**
 * Get the locale based week start from the load time data.
 * @returns {number}
 */
util.getLocaleBasedWeekStart = () => {
  return loadTimeData.valueExists('WEEK_START_FROM') ?
      loadTimeData.getInteger('WEEK_START_FROM') :
      0;
};

/**
 * A kind of error that represents user electing to cancel an operation. We use
 * this specialization to differentiate between system errors and errors
 * generated through legitimate user actions.
 */
class UserCanceledError extends Error {}

/**
 * Returns whether the given value is null or undefined.
 * @param {*} value
 * @returns {boolean}
 */
util.isNullOrUndefined = (value) => value === null || value === undefined;

/**
 * Bulk pinning should only show visible UI elements when in progress or
 * continuing to sync.
 * @param {chrome.fileManagerPrivate.BulkPinStage|undefined} stage
 * @param {boolean|undefined} pref
 * @returns {boolean}
 */
util.canBulkPinningCloudPanelShow = (stage, pref) => {
  if (!isDriveFsBulkPinningEnabled()) {
    return false;
  }

  const BulkPinStage = chrome.fileManagerPrivate.BulkPinStage;
  // If the stage is in progress and the bulk pinning preference is enabled,
  // then the cloud panel should not be visible.
  if (pref &&
      (stage === BulkPinStage.GETTING_FREE_SPACE ||
       stage === BulkPinStage.LISTING_FILES ||
       stage === BulkPinStage.SYNCING)) {
    return true;
  }

  // For the PAUSED... states the preference should still be enabled, however,
  // for the latter the preference will have been disabled.
  if ((stage === BulkPinStage.PAUSED_OFFLINE && pref) ||
      (stage === BulkPinStage.PAUSED_BATTERY_SAVER && pref) ||
      stage === BulkPinStage.NOT_ENOUGH_SPACE) {
    return true;
  }

  return false;
};

/**
 * Converts seconds into a time remaining string.
 * @param {number} seconds
 * @returns {string}
 */
util.secondsToRemainingTimeString = (seconds) => {
  const locale = util.getCurrentLocaleOrDefault();
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
};

export {util, UserCanceledError};
