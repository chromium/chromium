// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
