// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helpers for APIs used within Files app.
 */

import {FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';

import {util} from './util.js';

/**
 * Calls the `fn` function which should expect the callback as last argument.
 *
 * Resolves with the result of the `fn`.
 *
 * Rejects if there is `chrome.runtime.lastError`.
 *
 * @param {!function(...?)} fn
 * @param {...?} args
 * @return {!Promise<?>}
 */
export async function promisify(fn, ...args) {
  return new Promise((resolve, reject) => {
    const callback = (result) => {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError.message);
      } else {
        resolve(result);
      }
    };

    fn(...args, callback);
  });
}

/**
 * Opens a new window for Files SWA.
 *
 * @param {?chrome.fileManagerPrivate.OpenWindowParams=} params See
 *     file_manager_private.idl for details for `params` details.
 * @return {!Promise<boolean>}
 */
export async function openWindow(params) {
  return promisify(chrome.fileManagerPrivate.openWindow, params);
}

/**
 * @param {!Array<!Entry>} isolatedEntries entries to be resolved.
 * @return {!Promise<!Array<!Entry>>} entries resolved.
 */
export async function resolveIsolatedEntries(isolatedEntries) {
  return promisify(
      chrome.fileManagerPrivate.resolveIsolatedEntries, isolatedEntries);
}

/**
 * @return {!Promise<!chrome.fileManagerPrivate.Preferences|undefined>}
 */
export async function getPreferences() {
  return promisify(chrome.fileManagerPrivate.getPreferences);
}

/**
 * @param {!DirectoryEntry} parentEntry
 * @param {string} name
 * @return {!Promise<boolean>} True if valid, else throws.
 */
export async function validatePathNameLength(parentEntry, name) {
  return promisify(
      chrome.fileManagerPrivate.validatePathNameLength, parentEntry, name);
}

/**
 * Wrap the chrome.fileManagerPrivate.getSizeStats function in an async/await
 * compatible style.
 * @param {string} volumeId The volumeId to retrieve the size stats for.
 * @returns {!Promise<(!chrome.fileManagerPrivate.MountPointSizeStats|undefined)>}
 */
export async function getSizeStats(volumeId) {
  return promisify(chrome.fileManagerPrivate.getSizeStats, volumeId);
}

/**
 * Wrap the chrome.fileManagerPrivate.getDriveQuotaMetadata function in an
 * async/await compatible style.
 * @returns {!Promise<(
 * !chrome.fileManagerPrivate.DriveQuotaMetadata|undefined)>}
 */
export async function getDriveQuotaMetadata() {
  return promisify(chrome.fileManagerPrivate.getDriveQuotaMetadata);
}

/**
 * Retrieves the current holding space state, for example the list of items the
 * holding space currently contains.
 *  @returns {!Promise<(!chrome.fileManagerPrivate.HoldingSpaceState|undefined)>}
 */
export async function getHoldingSpaceState() {
  return promisify(chrome.fileManagerPrivate.getHoldingSpaceState);
}

/**
 * Wrap the chrome.fileManagerPrivate.getDisallowedTransfers function in an
 * async/await compatible style.
 * @param {!Array<!Entry>} entries entries to be transferred
 * @param {!DirectoryEntry} destinationEntry destination entry
 * @param {boolean} isMove true if the operation is move. false if copy.
 * @return {!Promise<!Array<!Entry>>} disallowed transfers
 */
export async function getDisallowedTransfers(
    entries, destinationEntry, isMove) {
  return promisify(
      chrome.fileManagerPrivate.getDisallowedTransfers,
      entries.map(e => util.unwrapEntry(e)), util.unwrapEntry(destinationEntry),
      isMove);
}

/**
 * Wrap the chrome.fileManagerPrivate.getDlpMetadata function in an async/await
 * compatible style.
 * @param {!Array<!Entry>} entries entries to be checked
 * @return {!Promise<!Array<!chrome.fileManagerPrivate.DlpMetadata>>} list of
 *     DlpMetadata
 */
export async function getDlpMetadata(entries) {
  return promisify(
      chrome.fileManagerPrivate.getDlpMetadata,
      entries.map(e => util.unwrapEntry(e)));
}

/**
 * Retrieves the list of components to which the transfer of an Entry is blocked
 * by Data Leak Prevention (DLP) policy.
 * @param {string} sourceUrl Source URL of the Entry that should be checked.
 * @return {!Promise<!Array<chrome.fileManagerPrivate.VolumeType>>}
 * callback Callback with the list of components (subset of VolumeType) to which
 * transferring an Entry is blocked by DLP.
 */
export async function getDlpBlockedComponents(sourceUrl) {
  return promisify(
      chrome.fileManagerPrivate.getDlpBlockedComponents, sourceUrl);
}

/**
 * Retrieves Data Leak Prevention (DLP) restriction details.
 * @param {string} sourceUrl Source URL of the file for which to retrieve the
 *     details.
 * @return {!Promise<!Array<!chrome.fileManagerPrivate.DlpRestrictionDetails>>}
 *     list of DlpRestrictionDetails containing summarized restriction
 * information about the file.
 */
export async function getDlpRestrictionDetails(sourceUrl) {
  return promisify(
      chrome.fileManagerPrivate.getDlpRestrictionDetails, sourceUrl);
}

/**
 * Retrieves the caller that created the dialog (Save As/File Picker).
 * @return {!Promise<!chrome.fileManagerPrivate.DialogCallerInformation>}
 * callback Callback with either a URL or component (subset of VolumeType) of
 * the caller.
 */
export async function getDialogCaller() {
  return promisify(chrome.fileManagerPrivate.getDialogCaller);
}

/**
 * Lists Guest OSs which support having their files mounted.
 * @return {!Promise<!Array<!chrome.fileManagerPrivate.MountableGuest>>}
 */
export async function listMountableGuests() {
  return promisify(chrome.fileManagerPrivate.listMountableGuests);
}

/**
 * Lists Guest OSs which support having their files mounted.
 * @param {number} id Id of the guest to mount.
 * @return {!Promise<void>}
 */
export async function mountGuest(id) {
  return promisify(chrome.fileManagerPrivate.mountGuest, id);
}

/*
 * FileSystemEntry helpers
 */

/**
 * @param {!Entry} entry
 * @return {!Promise<!DirectoryEntry>}
 */
export async function getParentEntry(entry) {
  return new Promise((resolve, reject) => {
    entry.getParent(resolve, reject);
  });
}

/**
 * @param {!Entry} entry
 * @param {!DirectoryEntry} parent
 * @param {string} newName
 * @return {!Promise<!Entry>}
 */
export async function moveEntryTo(entry, parent, newName) {
  return new Promise((resolve, reject) => {
    entry.moveTo(parent, newName, resolve, reject);
  });
}

/**
 * @param {!DirectoryEntry} directory
 * @param {string} filename
 * @param {!Object=} options
 * @return {!Promise<!FileEntry>}
 */
export async function getFile(directory, filename, options) {
  return new Promise((resolve, reject) => {
    directory.getFile(filename, options, resolve, reject);
  });
}

/**
 * @param {!DirectoryEntry} directory
 * @param {string} filename
 * @param {!Object=} options
 * @return {!Promise<!DirectoryEntry>}
 */
export async function getDirectory(directory, filename, options) {
  return new Promise((resolve, reject) => {
    directory.getDirectory(filename, options, resolve, reject);
  });
}

/**
 * @param {!DirectoryEntry} directory
 * @param {string} filename
 * @param {boolean} isFile Whether to retrieve a file or a directory.
 * @param {!Object=} options
 * @return {!Promise<!Entry>}
 */
export async function getEntry(directory, filename, isFile, options) {
  const getEntry = isFile ? getFile : getDirectory;
  return getEntry(directory, filename, options);
}

/**
 * Starts an IOTask of `type` and returns a taskId that can be used to cancel
 * or identify the ongoing IO operation.
 * @param {!chrome.fileManagerPrivate.IOTaskType} type
 * @param {!Array<!Entry|!FilesAppEntry>} entries
 * @param {!chrome.fileManagerPrivate.IOTaskParams} params
 * @returns {!Promise<!number>}
 */
export async function startIOTask(type, entries, params) {
  return promisify(
      chrome.fileManagerPrivate.startIOTask, type,
      entries.map(e => util.unwrapEntry(e)), params);
}

/**
 * Parses .trashinfo files to retrieve the restore path and deletion date.
 * @param {!Array<!Entry>} entries
 * @returns {!Promise<!Array<!chrome.fileManagerPrivate.ParsedTrashInfoFile>>}
 */
export async function parseTrashInfoFiles(entries) {
  return promisify(
      chrome.fileManagerPrivate.parseTrashInfoFiles,
      entries.map(e => util.unwrapEntry(e)));
}

/**
 * @param {!Entry} entry
 * @return {!Promise<string|undefined>}
 */
export async function getMimeType(entry) {
  return promisify(
      chrome.fileManagerPrivate.getMimeType, util.unwrapEntry(entry));
}

/**
 * @param {!Array<!Entry|!FilesAppEntry>} entries
 * @return {!Promise<chrome.fileManagerPrivate.ResultingTasks>}
 */
export async function getFileTasks(entries) {
  return promisify(chrome.fileManagerPrivate.getFileTasks, entries);
}

/**
 * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} taskDescriptor
 * @param {!Array<!Entry|!FilesAppEntry>} entries
 * @return {!Promise<chrome.fileManagerPrivate.TaskResult>}
 */
export async function executeTask(taskDescriptor, entries) {
  return promisify(
      chrome.fileManagerPrivate.executeTask, taskDescriptor, entries);
}

/**
 * Returns unique parent directories of provided entries. Note: this assumes
 * all provided entries are from the same filesystem.
 * @param {!Array<!Entry>} entries
 * @return {!Promise<!Array<!DirectoryEntry>>}
 */
export async function getUniqueParents(entries) {
  if (entries.length === 0) {
    return [];
  }
  const root = entries[0].filesystem.root;

  const uniquePaths = entries.reduce((paths, entry) => {
    const parts = entry.fullPath.split('/').slice(0, -1);

    while (parts.length > 1) {
      const path = parts.join('/');
      if (paths.has(path)) {
        return paths;
      }
      paths.add(path);
      parts.pop();
    }

    return paths;
  }, new Set());

  return Promise.all([...uniquePaths].map(path => getDirectory(root, path)));
}
