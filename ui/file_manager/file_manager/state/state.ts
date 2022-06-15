// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppDirEntry, FilesAppEntry} from '../externs/files_app_entry_interfaces.js';

import {FileKey} from './file_key.js';

export type AnyEntry = Entry|FilesAppEntry;
export type OnlyDirEntry = DirectoryEntry|FilesAppDirEntry;

/** The data for each individual file/entry. */
export interface FileData {
  entry: AnyEntry;
}

/**
 * Describes each part of the path, as in each parent folder and/or root volume.
 */
export interface PathComponent {
  // The actual name for folder/volume.
  name: string;

  // Label to display to the user, it might differ from the name when the folder
  // or volume has a translation or comercial name.
  label: string;

  // The key to the actual folder or root, it might be a key for a fake entry.
  key: FileKey;
}

/** The Current Directory. */
export interface CurrentDirectory {
  // Key to the current directory.
  key: FileKey;

  // Elements for the user facing path, e.g. the breadcrumbs.
  pathComponents: PathComponent[];
}

/** Files app's state. */
export interface State {
  // A big bucket with all entries managed by the app.
  allEntries: {
    [key: FileKey]: FileData,
  };

  // The currently selected directory.
  currentDirectory?: CurrentDirectory;
}
