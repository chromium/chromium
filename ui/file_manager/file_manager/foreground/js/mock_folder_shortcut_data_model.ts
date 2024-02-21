// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ArrayDataModel} from '../../common/js/array_data_model.js';
import type {MockEntry} from '../../common/js/mock_entry.js';

import type {FolderShortcutsDataModel} from './folder_shortcuts_data_model.js';

/**
 * Mock FolderShortcutDataModel.
 */
export class MockFolderShortcutDataModel extends ArrayDataModel {
  /**
   */
  constructor(array: unknown[]) {
    super(array);
  }

  /**
   * @public
   */
  asFolderShortcutsDataModel(): FolderShortcutsDataModel {
    return this as any as FolderShortcutsDataModel;
  }

  /**
   * Mock function for FolderShortcutDataModel.compare().
   * @param a First parameter to be compared.
   * @param b Second parameter to be compared with.
   * @return Negative if a < b, positive if a > b, or zero if a === b.
   */
  compare(a: MockEntry, b: MockEntry): number {
    return a.fullPath.localeCompare(b.fullPath);
  }
}
