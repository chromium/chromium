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
    // @ts-ignore: error TS18048: 'item' is possibly 'undefined'.
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
    // @ts-ignore: error TS7006: Parameter 'entries' implicitly has an 'any'
    // type.
    function entriesCallback(entries) {
      isMatchedEntryFound = true;
    }
    // @ts-ignore: error TS7006: Parameter 'error' implicitly has an 'any' type.
    function errorCallback(error) {
      console.warn(error.stack || error);
    }

    // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry' is not
    // assignable to parameter of type 'FileSystemDirectoryEntry |
    // FilesAppEntry'.
    const scanner = dirModel.createScannerFactory(dirEntry, searchQuery)();
    await new Promise(
        // @ts-ignore: error TS2345: Argument of type '(value: any) => void' is
        // not assignable to parameter of type '() => any'.
        resolve => scanner.scan(entriesCallback, resolve, errorCallback));
    if (isMatchedEntryFound) {
      // @ts-ignore: error TS2322: Type 'FileSystemEntry' is not assignable to
      // type 'FileSystemDirectoryEntry'.
      return dirEntry;
    }
  }
  // @ts-ignore: error TS2322: Type 'null' is not assignable to type
  // 'FileSystemDirectoryEntry'.
  return null;
};

export {crossoverSearchUtils};
