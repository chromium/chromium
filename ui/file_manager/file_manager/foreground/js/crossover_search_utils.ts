// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeManager} from '../../background/js/volume_manager.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';

import type {DirectoryModel} from './directory_model.js';

/**
 * Return DirectoryEntry of the first root directory (all volume display root
 * directories) that contains one or more query-matched files, returns null if
 * no such directory is found.
 *
 * @param volumeManager The volume manager.
 * @param dirModel The directory model.
 * @param searchQuery Search query.
 */
export async function findQueryMatchedDirectoryEntry(
    volumeManager: VolumeManager, dirModel: DirectoryModel,
    searchQuery: string): Promise<DirectoryEntry|FilesAppEntry|null> {
  for (let i = 0; i < volumeManager.volumeInfoList.length; i++) {
    const volumeInfo = volumeManager.volumeInfoList.item(i);
    // Make sure the volume root is resolved before scanning.
    await volumeInfo.resolveDisplayRoot();
    const dirEntry = volumeInfo.displayRoot;

    let isEntryFound = false;
    function entriesCallback() {
      isEntryFound = true;
    }
    function errorCallback(error: Error) {
      console.warn(error.stack || error);
    }

    const scanner = dirModel.createScannerFactory(
        dirEntry.toURL(), dirEntry, searchQuery)();
    await new Promise<void>(
        resolve => scanner.scan(entriesCallback, resolve, errorCallback));
    if (isEntryFound) {
      return dirEntry;
    }
  }
  return null;
}
