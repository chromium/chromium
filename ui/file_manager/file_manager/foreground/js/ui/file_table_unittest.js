// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!FileTableColumnModel} */
let model;

/** @type {!Array<cr.ui.table.TableColumn>} */
let columns;

function setUp() {
  columns = [
    new cr.ui.table.TableColumn('col0', 'col0', 100),
    new cr.ui.table.TableColumn('col1', 'col1', 100),
    new cr.ui.table.TableColumn('col2', 'col2', 100),
    new cr.ui.table.TableColumn('col3', 'col3', 100),
    new cr.ui.table.TableColumn('col4', 'col4', 100)
  ];

  model = new FileTableColumnModel(columns);
}

/**
 * Extracts column widths from the model.
 * @param {!FileTableColumnModel} model
 */
function getColumnWidths(model) {
  const widths = [];
  for (let i = 0; i < model.size; i++) {
    widths[i] = model.getWidth(i);
  }
  return widths;
}

// Verify that column visibility and width are correctly set when the visibility
// setting is toggled.
function testToggleVisibility() {
  // The column under test.
  const INDEX = 2;
  const width = model.getWidth(INDEX);

  // All columns should be visible to start.
  for (let i = 0; i < model.size; i++) {
    assertTrue(model.isVisible(i));
  }

  // Test toggling visibility.
  model.setVisible(INDEX, false);
  assertFalse(model.isVisible(INDEX));
  assertEquals(0, model.getWidth(INDEX));

  model.setVisible(INDEX, true);
  assertTrue(model.isVisible(INDEX));
  assertEquals(width, model.getWidth(INDEX));
}

// Verify that the table layout does not drift when a column is repeatedly shown
// and hidden.
function testToggleVisibilityColumnLayout() {
  // The index of the column under test.
  const INDEX = 2;
  // Capture column widths.
  const visibleWidths = getColumnWidths(model);
  // Total width should be invariant throughout.
  const totalWidth = model.totalWidth;

  // Hide a column, check total width.
  model.setVisible(INDEX, false);
  assertEquals(totalWidth, model.totalWidth);
  const hiddenWidths = getColumnWidths(model);

  // Show the column again, match the column widths to the original state.
  model.setVisible(INDEX, true);
  assertArrayEquals(visibleWidths, getColumnWidths(model));
  assertEquals(totalWidth, model.totalWidth);

  // Hide the column again, match the column widths to the hidden state.
  model.setVisible(INDEX, false);
  assertArrayEquals(hiddenWidths, getColumnWidths(model));
  assertEquals(totalWidth, model.totalWidth);
}

// Verify that table layout stays constant when the column config is exported
// and then restored, with no hidden columns.
function testExportAndRestoreColumnConfigWithNoHiddenColumns() {
  // Change some column widths, then capture then.
  for (let i = 0; i < model.size; i++) {
    model.setWidth(i, i * 50);
  }
  const expectedWidths = getColumnWidths(model);
  const expectedTotalWidth = model.totalWidth;

  // Export column config, restore it to the new model.
  const config = model.exportColumnConfig();

  const newModel = new FileTableColumnModel(columns);
  newModel.restoreColumnConfig(config);
  assertArrayEquals(expectedWidths, getColumnWidths(newModel));
  assertEquals(expectedTotalWidth, newModel.totalWidth);
}

// Verify that table layout stays constant when the column config is exported
// and then restored, with a hidden column.
function testExportAndRestoreColumnConfigWithHiddenColumns() {
  // The index of the column under test.
  const INDEX = 2;

  // Change some column widths, then capture then.
  for (let i = 0; i < model.size; i++) {
    model.setWidth(i, (i + 1) * 50);
  }
  // Hide a column.
  model.setVisible(INDEX, false);
  const expectedWidths = getColumnWidths(model);
  const expectedTotalWidth = model.totalWidth;

  // Export column config, restore it to the new model.
  const config = model.exportColumnConfig();

  const newModel = new FileTableColumnModel(columns);
  // Hide the same column.
  newModel.setVisible(INDEX, false);
  newModel.restoreColumnConfig(config);

  assertArrayEquals(expectedWidths, getColumnWidths(newModel));
  assertEquals(expectedTotalWidth, newModel.totalWidth);
}

// Verify that table layout stays constant when the column config is exported
// with a hidden column but then restored with the column visible.
function testExportAndRestoreColumnConfigWithShowingColumn() {
  // The index of the column under test.
  const INDEX = 2;

  // Change some column widths, then capture then.
  for (let i = 0; i < model.size; i++) {
    model.setWidth(i, (i + 1) * 50);
  }
  // Hide a column.
  model.setVisible(INDEX, false);
  const expectedWidths = getColumnWidths(model);
  const expectedTotalWidth = model.totalWidth;

  // Export column config, restore it to the new model.
  const config = model.exportColumnConfig();

  const newModel = new FileTableColumnModel(columns);
  // Restore column config while the test column is shown.
  newModel.setVisible(INDEX, true);
  newModel.restoreColumnConfig(config);
  // Then hide it.
  newModel.setVisible(INDEX, false);

  assertArrayEquals(expectedWidths, getColumnWidths(newModel));
  assertEquals(expectedTotalWidth, newModel.totalWidth);
}

// Verify that table layout stays constant when the column config is exported
// with all columns visible but then restored with a hidden column.
function testExportAndRestoreColumnConfigWithHidingColumn() {
  // The index of the column under test.
  const INDEX = 2;

  // Change some column widths, then capture then.
  for (let i = 0; i < model.size; i++) {
    model.setWidth(i, (i + 1) * 50);
  }
  // Verify the precondition.
  assertTrue(model.isVisible(INDEX));
  const expectedWidths = getColumnWidths(model);
  const expectedTotalWidth = model.totalWidth;

  // Export column config, restore it to the new model.
  const config = model.exportColumnConfig();

  const newModel = new FileTableColumnModel(columns);
  // Restore column config while the test column is hidden.
  newModel.setVisible(INDEX, false);
  newModel.restoreColumnConfig(config);
  // Then show it.
  newModel.setVisible(INDEX, true);

  assertArrayEquals(expectedWidths, getColumnWidths(newModel));
  assertEquals(expectedTotalWidth, newModel.totalWidth);
}

function testNormalizeWidth() {
  const newContentWidth = 150;
  const expectedWidths = [10, 20, 30, 40, 50];

  for (let i = 0; i < model.size; i++) {
    model.setWidth(i, expectedWidths[i] * 17);
  }

  // Resizes columns proportionally
  model.normalizeWidths(newContentWidth);

  assertArrayEquals(expectedWidths, getColumnWidths(model));
  assertEquals(newContentWidth, model.totalWidth);
}

function testNormalizeWidthWithSmallWidth() {
  model.normalizeWidths(10);  // not enough width to contain all columns

  // Should keep the minimum width.
  getColumnWidths(model).map(width => {
    assertEquals(FileTableColumnModel.MIN_WIDTH_, width);
  });
}

function testSetWidthAndKeepTotal() {
  // Make sure to take column snapshot. Required for setWidthAndKeepTotal.
  model.initializeColumnPos();

  // Attempt to expand the 3rd column exceeding the window.
  model.setWidthAndKeepTotal(2, 400);

  // Should keep the minimum width.
  getColumnWidths(model).map(width => {
    assertTrue(width >= FileTableColumnModel.MIN_WIDTH_);
  });
  const minWidth = FileTableColumnModel.MIN_WIDTH_;
  // Total width = 500.
  const expectedWidths =
      [100, 100, 500 - 100 * 2 - minWidth * 2, minWidth, minWidth];
  assertArrayEquals(expectedWidths, getColumnWidths(model));
}
