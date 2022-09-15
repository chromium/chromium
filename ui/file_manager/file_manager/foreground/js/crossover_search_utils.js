// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DirectoryModel} from './directory_model.js';
import {NavigationListModel, NavigationModelFakeItem, NavigationModelItemType, NavigationModelVolumeItem} from './navigation_list_model.js';

/**
 * Namespace for crossover search utility functions.
 * @namespace
 */
const crossoverSearchUtils = {};

/**
 * Return DirectoryEntry of the first root directory that contains one or more
 *     query-matched files.
 * @param {NavigationListModel} navListModel
 * @param {DirectoryModel} dirModel
 * @param {string} searchQuery Search query.
 * @return {!Promise<DirectoryEntry>} DirectoryEntry of the first root
 *     directory (of type ENTRY_LIST or VOLUME) containing one or more files
 *     that match the search query.
 *     Returns null if no such directory is found.
 */
crossoverSearchUtils.findQueryMatchedDirectoryEntry =
    async (navListModel, dirModel, searchQuery) => {
  for (let i = 0; i < navListModel.length; i++) {
    const item = navListModel.item(i);
    let dirEntry;
    switch (item.type) {
      case NavigationModelItemType.ENTRY_LIST:  // My files, Removable, etc.
        dirEntry = /** @type {NavigationModelFakeItem} */
            (item).entry.getNativeEntry();
        break;
      case NavigationModelItemType.VOLUME:  // Drive, DocumentsProvider, etc.
        dirEntry = /** @type {NavigationModelVolumeItem} */
            (item).volumeInfo.displayRoot;
        break;
      default:
        continue;
    }
    if (!dirEntry) {
      continue;
    }

    let isMatchedEntryFound;
    function entriesCallback(entries) {
      isMatchedEntryFound = true;
    }
    function errorCallback(error) {
      console.warn(error.stack || error);
    }

    const scanner = dirModel.createScannerFactory(dirEntry, searchQuery)();
    await new Promise(
        resolve => scanner.scan(entriesCallback, resolve, errorCallback));
    if (isMatchedEntryFound) {
      return dirEntry;
    }
  }
  return null;
};

export {crossoverSearchUtils};
