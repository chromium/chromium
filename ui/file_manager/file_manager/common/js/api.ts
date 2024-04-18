// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Helpers for APIs used within Files app.
 */

import type {FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import type {FileKey} from '../../state/state.js';

import {unwrapEntry, urlToEntry} from './entry_utils.js';
import {promisify} from './util.js';

/**
 * Opens a new window for Files SWA.
 */
export async function openWindow(
    params: chrome.fileManagerPrivate.OpenWindowParams|undefined) {
  return promisify<boolean>(chrome.fileManagerPrivate.openWindow, params);
}

export async function resolveIsolatedEntries(isolatedEntries: Entry[]) {
  return promisify<Entry[]>(
      chrome.fileManagerPrivate.resolveIsolatedEntries,
      isolatedEntries.map(e => unwrapEntry(e)));
}

export async function getPreferences() {
  return promisify<chrome.fileManagerPrivate.Preferences>(
      chrome.fileManagerPrivate.getPreferences);
}

export async function validatePathNameLength(
    parentEntry: FilesAppDirEntry|DirectoryEntry, name: string) {
  return promisify<boolean>(
      chrome.fileManagerPrivate.validatePathNameLength,
      unwrapEntry(parentEntry), name);
}

/**
 * Wrap the chrome.fileManagerPrivate.getSizeStats function in an async/await
 * compatible style.
 */
export async function getSizeStats(volumeId: string) {
  return promisify<chrome.fileManagerPrivate.MountPointSizeStats|undefined>(
      chrome.fileManagerPrivate.getSizeStats, volumeId);
}

/**
 * Wrap the chrome.fileManagerPrivate.getDriveQuotaMetadata function in an
 * async/await compatible style.
 */
export async function getDriveQuotaMetadata(entry: Entry|FilesAppEntry) {
  return promisify<chrome.fileManagerPrivate.DriveQuotaMetadata|undefined>(
      chrome.fileManagerPrivate.getDriveQuotaMetadata, unwrapEntry(entry));
}

/**
 * Retrieves the current holding space state, for example the list of items the
 * holding space currently contains.
 */
export async function getHoldingSpaceState() {
  return promisify<chrome.fileManagerPrivate.HoldingSpaceState|undefined>(
      chrome.fileManagerPrivate.getHoldingSpaceState);
}

/**
 * Wrap the chrome.fileManagerPrivate.getDisallowedTransfers function in an
 * async/await compatible style.
 */
export async function getDisallowedTransfers(
    entries: Entry[], destinationEntry: DirectoryEntry|FilesAppDirEntry,
    isMove: boolean) {
  return promisify<Entry[]>(
      chrome.fileManagerPrivate.getDisallowedTransfers,
      entries.map(e => unwrapEntry(e)), unwrapEntry(destinationEntry), isMove);
}

/**
 * Wrap the chrome.fileManagerPrivate.getDlpMetadata function in an async/await
 * compatible style.
 */
export async function getDlpMetadata(entries: Array<Entry|FilesAppEntry>) {
  return promisify<chrome.fileManagerPrivate.DlpMetadata[]>(
      chrome.fileManagerPrivate.getDlpMetadata,
      entries.map(e => unwrapEntry(e)));
}

/**
 * Retrieves the list of components to which the transfer of an Entry is blocked
 * by Data Leak Prevention (DLP) policy.
 */
export async function getDlpBlockedComponents(sourceUrl: string) {
  return promisify<chrome.fileManagerPrivate.VolumeType[]>(
      chrome.fileManagerPrivate.getDlpBlockedComponents, sourceUrl);
}

/**
 * Retrieves Data Leak Prevention (DLP) restriction details.
 */
export async function getDlpRestrictionDetails(sourceUrl: string) {
  return promisify<chrome.fileManagerPrivate.DlpRestrictionDetails[]>(
      chrome.fileManagerPrivate.getDlpRestrictionDetails, sourceUrl);
}

/**
 * Retrieves the caller that created the dialog (Save As/File Picker).
 */
export async function getDialogCaller() {
  return promisify<chrome.fileManagerPrivate.DialogCallerInformation>(
      chrome.fileManagerPrivate.getDialogCaller);
}

/**
 * Lists Guest OSs which support having their files mounted.
 */
export async function listMountableGuests() {
  return promisify<chrome.fileManagerPrivate.MountableGuest[]>(
      chrome.fileManagerPrivate.listMountableGuests);
}

/**
 * Lists Guest OSs which support having their files mounted.
 */
export async function mountGuest(id: number) {
  return promisify<void>(chrome.fileManagerPrivate.mountGuest, id);
}

/*
 * FileSystemEntry helpers
 */

export async function getParentEntry(entry: Entry|
                                     FilesAppEntry): Promise<DirectoryEntry> {
  return new Promise((resolve, reject) => {
    entry.getParent(resolve, reject);
  });
}

export async function moveEntryTo(
    entry: Entry|FilesAppEntry, parent: DirectoryEntry,
    newName: string): Promise<Entry|FilesAppEntry> {
  return new Promise((resolve, reject) => {
    entry.moveTo(parent, newName, resolve, reject);
  });
}

export async function getFile(
    directory: FilesAppDirEntry|DirectoryEntry, filename: string,
    options: Flags|undefined): Promise<FileEntry> {
  return new Promise((resolve, reject) => {
    directory.getFile(filename, options, resolve, reject);
  });
}

export async function getDirectory(
    directory: DirectoryEntry, filename: string,
    options: Flags|undefined): Promise<DirectoryEntry> {
  return new Promise((resolve, reject) => {
    directory.getDirectory(filename, options, resolve, reject);
  });
}

export async function getEntry(
    directory: DirectoryEntry, filename: string, isFile: boolean,
    options?: Flags): Promise<Entry> {
  const getEntry = isFile ? getFile : getDirectory;
  return getEntry(directory, filename, options);
}

/**
 * Starts an IOTask of `type` and returns a taskId that can be used to cancel
 * or identify the ongoing IO operation.
 */
export async function startIOTask(
    type: chrome.fileManagerPrivate.IoTaskType,
    entries: Array<Entry|FilesAppEntry>,
    params: chrome.fileManagerPrivate.IoTaskParams) {
  if (params.destinationFolder) {
    params.destinationFolder =
        unwrapEntry(params.destinationFolder) as DirectoryEntry;
  }
  return promisify<number>(
      chrome.fileManagerPrivate.startIOTask, type,
      entries.map(e => unwrapEntry(e)), params);
}

/**
 * Parses .trashinfo files to retrieve the restore path and deletion date.
 */
export async function parseTrashInfoFiles(entries: Entry[]) {
  return promisify<chrome.fileManagerPrivate.ParsedTrashInfoFile[]>(
      chrome.fileManagerPrivate.parseTrashInfoFiles,
      entries.map(e => unwrapEntry(e)));
}

export async function getMimeType(entry: Entry|FilesAppEntry) {
  return chrome.fileManagerPrivate.getMimeType(entry.toURL());
}

export async function getFileTasks(
    entries: Array<Entry|FilesAppEntry>, dlpSourceUrls: string[]) {
  return promisify<chrome.fileManagerPrivate.ResultingTasks>(
      chrome.fileManagerPrivate.getFileTasks, entries.map(e => unwrapEntry(e)),
      dlpSourceUrls);
}

export async function executeTask(
    taskDescriptor: chrome.fileManagerPrivate.FileTaskDescriptor,
    entries: Array<Entry|FilesAppEntry>) {
  return promisify<chrome.fileManagerPrivate.TaskResult>(
      chrome.fileManagerPrivate.executeTask, taskDescriptor,
      entries.map(e => unwrapEntry(e)));
}

/**
 * Returns unique parent directories of provided entries. Note: this assumes
 * all provided entries are from the same filesystem.
 */
export async function getUniqueParents(entries: Entry[]):
    Promise<DirectoryEntry[]> {
  if (entries.length === 0) {
    return [];
  }
  const root = entries[0]!.filesystem.root;

  const uniquePaths = entries.reduce((paths: Set<string>, entry: Entry) => {
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

  return Promise.all([...uniquePaths].map(
      (path: string) => getDirectory(root, path, undefined)));
}

/**
 * Gets the current bulk pin progress status.
 */
export async function getBulkPinProgress() {
  return promisify<chrome.fileManagerPrivate.BulkPinProgress>(
      chrome.fileManagerPrivate.getBulkPinProgress);
}

/**
 * Starts calculating the required space to pin all the users items on their My
 * drive.
 */
export async function calculateBulkPinRequiredSpace() {
  return promisify<void>(
      chrome.fileManagerPrivate.calculateBulkPinRequiredSpace);
}

/**
 * Wrap the chrome.fileManagerPrivate.getDriveConnectionStatus function in an
 * async/await compatible style.
 */
export async function getDriveConnectionState() {
  return promisify<chrome.fileManagerPrivate.DriveConnectionState>(
      chrome.fileManagerPrivate.getDriveConnectionState);
}

export async function grantAccess(entries: string[]) {
  return promisify<void>(chrome.fileManagerPrivate.grantAccess, entries);
}

export async function getContentMimeType(fileEntry: FileEntry) {
  return promisify<string>(
      chrome.fileManagerPrivate.getContentMimeType, fileEntry);
}

export async function getContentMetadata(
    fileEntry: FileEntry, mimeType: string, includeImages: boolean) {
  return promisify<chrome.fileManagerPrivate.MediaMetadata>(
      chrome.fileManagerPrivate.getContentMetadata, fileEntry, mimeType,
      includeImages);
}

export async function getEntryProperties(
    entries: Array<Entry|FilesAppEntry>,
    properties: chrome.fileManagerPrivate.EntryPropertyName[]):
    Promise<chrome.fileManagerPrivate.EntryProperties[]> {
  return promisify(
      chrome.fileManagerPrivate.getEntryProperties, entries.map(unwrapEntry),
      properties);
}

export async function getMaterializedViews():
    Promise<chrome.fileManagerPrivate.MaterializedView[]> {
  try {
    const views = await chrome.fileManagerPrivate.getMaterializedViews();
    return views;
  } catch (error: any) {
    console.warn(error);
    return [];
  }
}

export async function readMaterializedView(fileKey: FileKey): Promise<Entry[]> {
  const url = new URL(fileKey);
  const viewId = parseInt(url.pathname.replace('//', '').split('/')[0]!);
  const entryData =
      await chrome.fileManagerPrivate.readMaterializedView(viewId);
  const entries = await Promise.all(entryData.map(e => urlToEntry(e.entryUrl)));
  return entries;
}
