// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {importer} from '../../common/js/importer_common.js';
import {importerHistoryInterfaces} from './import_history.js';

// Namespace
export const duplicateFinderInterfaces = {};

/**
 * Declare DispositionChecker class.
 * @interface
 */
duplicateFinderInterfaces.DispositionChecker = class {
  /**
   * Factory for a function that returns a file entry's content disposition.
   *
   * @param {!importerHistoryInterfaces.HistoryLoader} historyLoader
   *
   * @return {!duplicateFinderInterfaces.DispositionChecker.CheckerFunction}
   */
  static createChecker(historyLoader) {}
};

/**
 * Define a function type that returns a Promise that resolves the content
 * disposition of an entry.
 *
 * @typedef {function(!FileEntry, !importer.Destination, !importer.ScanMode):
 *     !Promise<!importer.Disposition>}
 */
duplicateFinderInterfaces.DispositionChecker.CheckerFunction;
