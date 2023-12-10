// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertArrayEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {FileListSelectionModel} from './file_list_selection_model.js';

let selectionModel: FileListSelectionModel;

export function setUp() {
  selectionModel = new FileListSelectionModel();
}

// Verify that all selection and focus is dropped if all selected files get
// deleted.
export function testAdjustToReorderingAllAreDeleted() {
  // Set initial selection.
  selectionModel.selectedIndexes = [0, 1];
  // Delete the selected items.
  selectionModel.adjustToReordering([-1, -1, 0]);
  // Assert nothing is selected or in focus.
  assertArrayEquals([], selectionModel.selectedIndexes);
  assertFalse(selectionModel.getCheckSelectMode());
}

// Verify that all selection and focus is dropped only if all selected files get
// deleted.
export function testAdjustToReorderingSomeAreDeleted() {
  // Set initial selection.
  selectionModel.selectedIndexes = [0, 1];
  // Delete the selected items.
  selectionModel.adjustToReordering([-1, 0, 1]);
  // Assert selection is not dropped.
  assertArrayEquals([0], selectionModel.selectedIndexes);
  assertTrue(selectionModel.getCheckSelectMode());
}
