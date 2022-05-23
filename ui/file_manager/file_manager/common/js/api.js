// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helpers for APIs used within Files app.
 */

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
 * @return {!Promise<!Array<!Entry>>} disallowed transfers
 */
export async function getDisallowedTransfers(entries, destinationEntry) {
  return promisify(
      chrome.fileManagerPrivate.getDisallowedTransfers, entries,
      destinationEntry);
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
 * Returns the color to be used by frames of each foreground window.
 * @returns {Promise<!string>}
 */
export async function getFrameColor() {
  try {
    return await promisify(chrome.fileManagerPrivate.getFrameColor);
  } catch (e) {
    console.error('Failed to get frame color.', e);
    return '#ffffff';
  }
}

/**
 * Starts an IOTask of `type` and returns a taskId that can be used to cancel
 * or identify the ongoing IO operation.
 * @param {!chrome.fileManagerPrivate.IOTaskType} type
 * @param {!Array<!Entry>} entries
 * @param {!chrome.fileManagerPrivate.IOTaskParams} params
 * @returns {!Promise<!number>}
 */
export async function startIOTask(type, entries, params) {
  return promisify(
      chrome.fileManagerPrivate.startIOTask, type, entries, params);
}
