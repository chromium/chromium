// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Typings for the `directory-changed` event that gets dispatched
 * from the DirectoryModel. Remove this file when the DirectoryModel has been
 * converted as the event belongs there.
 */

import type {FilesAppDirEntry} from '../externs/files_app_entry_interfaces.js';

export type DirectoryChangeEvent = CustomEvent<{
  previousDirEntry: DirectoryEntry | FilesAppDirEntry | FakeEntry,
  newDirEntry: DirectoryEntry | FilesAppDirEntry | FakeEntry,
  volumeChanged: boolean,
}>;
