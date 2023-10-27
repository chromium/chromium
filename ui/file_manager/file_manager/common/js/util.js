// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file should contain utility functions used only by the
 * files app. Other shared utility functions can be found in base/*_util.js,
 * which allows finer-grained control over introducing dependencies.
 */

import {promisify} from './api.js';
import {isDriveFsBulkPinningEnabled} from './flags.js';

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

export {util, UserCanceledError};
