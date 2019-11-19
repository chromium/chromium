// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Namespace
var importer = importer || {};

/**
 * Declare DispositionChecker class.
 * @interface
 */
importer.DispositionChecker = class {
  /**
   * Factory for a function that returns a file entry's content disposition.
   *
   * @param {!importer.HistoryLoader} historyLoader
   *
   * @return {!importer.DispositionChecker.CheckerFunction}
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
importer.DispositionChecker.CheckerFunction;
