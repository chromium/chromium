// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EntryType, FileData} from '../../externs/ts/state.js';

import type {VolumeEntry} from './files_app_entry_types.js';

/**
 * Type guard used to identify if a generic FileSystemEntry is actually a
 * FileSystemDirectoryEntry.
 */
export function isFileSystemDirectoryEntry(entry: FileSystemEntry):
    entry is FileSystemDirectoryEntry {
  return entry.isDirectory;
}

/**
 * Type guard used to identify if a generic FileSystemEntry is actually a
 * FileSystemFileEntry.
 */
export function isFileSystemFileEntry(entry: FileSystemEntry):
    entry is FileSystemFileEntry {
  return entry.isFile;
}

/**
 * Returns the native entry (aka FileEntry) from the Store. It returns `null`
 * for entries that aren't native.
 */
export function getNativeEntry(fileData: FileData): Entry|null {
  if (fileData.type === EntryType.FS_API) {
    return fileData.entry as Entry;
  }
  if (fileData.type === EntryType.VOLUME_ROOT) {
    return (fileData.entry as VolumeEntry).getNativeEntry();
  }
  return null;
}
