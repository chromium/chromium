// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../../background/js/mock_volume_manager.js';
import {FakeEntryImpl} from '../../../common/js/files_app_entry_types.js';
import {RootType} from '../../../common/js/volume_manager_types.js';
import {FileListModel, GROUP_BY_FIELD_DIRECTORY, GROUP_BY_FIELD_MODIFICATION_TIME} from '../file_list_model.js';
import type {MetadataModel} from '../metadata/metadata_model.js';
import {MockMetadataModel} from '../metadata/mock_metadata.js';

import type {A11yAnnounce} from './a11y_announce.js';
import {FileGrid, FileGridSelectionController} from './file_grid.js';
import {ListSelectionModel} from './list_selection_model.js';

let volumeManager: MockVolumeManager;
let metadataModel: MetadataModel;
let element: HTMLElement;
let a11y: A11yAnnounce;

/**
 * Returns the element used to parent the file grid. The element is
 * attached to the body, and styled for visual display.
 */
function setupBody(): HTMLElement {
  document.body.innerHTML = getTrustedHTML`
    <style>
      grid {
        display: block;
        height: 200px;
        width: 800px;
      }
    </style>
  `;

  const element = document.createElement('div');
  document.body.appendChild(element);
  return element;
}

// Set up test components.
export function setUp() {
  // Setup mock components.
  volumeManager = new MockVolumeManager();
  metadataModel = new MockMetadataModel({}) as unknown as MetadataModel;

  const a11Messages = [];
  a11y = {
    speakA11yMessage: (text) => {
      a11Messages.push(text);
    },
  };

  // Create DOM element parent of the file grid under test.
  element = setupBody();
}

// Force round number heights to simplify the math in the test.
const FILE_ITEM_HEIGHT = 50;
const FOLDER_ITEM_HEIGHT = 20;
const GROUP_HEADING_HEIGHT = 30;
const ITEM_WIDTH = 100;
const ITEM_MARGIN_TOP = 10;
const ITEM_MARGIN_LEFT = 10;

function setupFileGrid(): FileGrid {
  FileGrid.decorate(element, metadataModel, volumeManager, a11y);

  // Add 10 fake files.
  const entries = [];
  for (let i = 1; i <= 20; i++) {
    entries.push(new FakeEntryImpl(`${i}.txt`, RootType.RECENT));
  }
  const dataModel = new FileListModel(metadataModel);
  dataModel.splice(0, 0, ...entries);
  const grid = element as FileGrid;
  grid.dataModel = dataModel;
  // Mock item size.
  grid['getFileItemHeight_'] = () => FILE_ITEM_HEIGHT;
  grid['getFolderItemHeight_'] = () => FOLDER_ITEM_HEIGHT;
  grid['getItemWidth_'] = () => ITEM_WIDTH;
  grid['getItemMarginTop_'] = () => ITEM_MARGIN_TOP;
  grid['getItemMarginLeft_'] = () => ITEM_MARGIN_LEFT;
  // 3 columns in each row.
  grid.columns = 3;
  return grid;
}

function groupByModificationTime(fileListModel: FileListModel) {
  const RecentDateBucket = chrome.fileManagerPrivate.RecentDateBucket;

  // Mock group by information.
  fileListModel.shouldShowGroupHeading = () => true;
  fileListModel.groupByField = GROUP_BY_FIELD_MODIFICATION_TIME;
  // Visual illustration of the grid (in total 8 rows with 6 headings):
  // -----------------------------------------------------------------
  // Heading #1/today:
  // (row 0)    Item 0    Item 1
  // Heading #2/yesterday:
  // (row 1)    Item 2    Item 3    Item 4
  // Heading #3/earlier_this_week:
  // (row 2)    Item 5    Item 6    Item 7
  // (row 3)    Item 8    Item 9
  // Heading #4/earlier_this_month:
  // (row 4)    Item 10   Item 11   Item 12
  // (row 5)    Item 13   Item 14   Item 15
  // Heading #5/earlier_this_year:
  // (row 6)    Item 16
  // Heading #6/older:
  // (row 7)    Item 17   Item 18   Item 19
  fileListModel.getGroupBySnapshot = () => {
    return [
      {
        startIndex: 0,
        endIndex: 1,
        label: 'today',
        group: RecentDateBucket.TODAY,
      },
      {
        startIndex: 2,
        endIndex: 4,
        label: 'yesterday',
        group: RecentDateBucket.YESTERDAY,
      },
      {
        startIndex: 5,
        endIndex: 9,
        label: 'earlier_this_week',
        group: RecentDateBucket.EARLIER_THIS_WEEK,
      },
      {
        startIndex: 10,
        endIndex: 15,
        label: 'earlier_this_month',
        group: RecentDateBucket.EARLIER_THIS_MONTH,
      },
      {
        startIndex: 16,
        endIndex: 16,
        label: 'earlier_this_year',
        group: RecentDateBucket.EARLIER_THIS_YEAR,
      },
      {
        startIndex: 17,
        endIndex: 19,
        label: 'older',
        group: RecentDateBucket.OLDER,
      },
    ];
  };
}

function groupByDirectory(fileListModel: FileListModel) {
  // Mock group by information.
  fileListModel.shouldShowGroupHeading = () => true;
  fileListModel.groupByField = GROUP_BY_FIELD_DIRECTORY;
  // Visual illustration of the grid (in total 8 rows with 2 headings):
  // -----------------------------------------------------------------
  // Heading #1/folders:
  // (row 0)    Item 0    Item 1    Item 2
  // (row 1)    Item 3
  // Heading #2/files:
  // (row 2)    Item 4    Item 5    Item 6
  // (row 3)    Item 7    Item 8    Item 9
  // (row 4)    Item 10   Item 11   Item 12
  // (row 5)    Item 13   Item 14   Item 15
  // (row 6)    Item 16   Item 17   Item 18
  // (row 7)    Item 19
  fileListModel.getGroupBySnapshot = () => {
    return [
      {startIndex: 0, endIndex: 3, label: 'folders', group: true},
      {startIndex: 4, endIndex: 19, label: 'files', group: false},
    ];
  };
}

export function testGetItemTop() {
  const grid = setupFileGrid();
  grid['getGroupHeadingHeight_'] = () => GROUP_HEADING_HEIGHT;
  const ROW_HEIGHT = FILE_ITEM_HEIGHT;
  assert(grid.dataModel);
  // Enable group by modification time.
  groupByModificationTime(grid.dataModel);
  // Item 0,1 is in group #1/today, nothing is above it.
  assertEquals(grid.getItemTop(0), 0);
  assertEquals(grid.getItemTop(1), 0);
  // Item 2,3,4 is in group #2/yesterday, 1 row above + 1 header.
  assertEquals(grid.getItemTop(2), 1 * ROW_HEIGHT + GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(3), 1 * ROW_HEIGHT + GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(4), 1 * ROW_HEIGHT + GROUP_HEADING_HEIGHT);
  // Item 5,6,7 is in group #3/earlier_this_week, 2 rows above + 2 headers.
  assertEquals(grid.getItemTop(5), 2 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(6), 2 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(7), 2 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  // Item 8,9 is in group #3/earlier_this_week, 3 rows above + 3 headers.
  assertEquals(grid.getItemTop(8), 3 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(9), 3 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 10,11,12 is in group #4/earlier_this_month, 4 rows above + 3 headers.
  assertEquals(grid.getItemTop(10), 4 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(11), 4 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(12), 4 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 13,14,15 is in group #4/earlier_this_month, 5 rows above + 4 headers.
  assertEquals(grid.getItemTop(13), 5 * ROW_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(14), 5 * ROW_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(15), 5 * ROW_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  // Item 16 is in group #5/earlier_this_year, 6 rows above + 4 headers.
  assertEquals(grid.getFirstItemInRow(6), 16);
  assertEquals(grid.getItemTop(16), 6 * ROW_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  // Item 17,18,19 is in group #6/older, 7 rows above + 5 header.
  assertEquals(grid.getFirstItemInRow(7), 17);
  assertEquals(grid.getItemTop(17), 7 * ROW_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(18), 7 * ROW_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
  assertEquals(grid.getItemTop(19), 7 * ROW_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
}

// Test functions related to item position: getItemRow(), getItemColumn(),
// getItemIndex(), getFirstItemInRow().
export function testGetItemPosition() {
  const grid = setupFileGrid();
  assert(grid.dataModel);
  // Enable group by modification time.
  groupByModificationTime(grid.dataModel);
  // Check the comment in groupByModificationTime() for a visual illustration.
  // Item 0,1 is in group #1/today, row 0.
  assertEquals(grid.getFirstItemInRow(0), 0);
  assertEquals(grid.getItemRow(0), 0);
  assertEquals(grid.getItemColumn(0), 0);
  assertEquals(grid.getItemIndex(0, 0), 0);
  assertEquals(grid.getItemRow(1), 0);
  assertEquals(grid.getItemColumn(1), 1);
  assertEquals(grid.getItemIndex(0, 1), 1);
  // Item 2,3,4 is in group #2/yesterday, row 1.
  assertEquals(grid.getFirstItemInRow(1), 2);
  assertEquals(grid.getItemRow(2), 1);
  assertEquals(grid.getItemColumn(2), 0);
  assertEquals(grid.getItemIndex(1, 0), 2);
  assertEquals(grid.getItemRow(3), 1);
  assertEquals(grid.getItemColumn(3), 1);
  assertEquals(grid.getItemIndex(1, 1), 3);
  assertEquals(grid.getItemRow(4), 1);
  assertEquals(grid.getItemColumn(4), 2);
  assertEquals(grid.getItemIndex(1, 2), 4);
  // Item 5,6,7 is in group #3/earlier_this_week, row 2.
  assertEquals(grid.getFirstItemInRow(2), 5);
  assertEquals(grid.getItemRow(5), 2);
  assertEquals(grid.getItemColumn(5), 0);
  assertEquals(grid.getItemIndex(2, 0), 5);
  assertEquals(grid.getItemRow(6), 2);
  assertEquals(grid.getItemColumn(6), 1);
  assertEquals(grid.getItemIndex(2, 1), 6);
  assertEquals(grid.getItemRow(7), 2);
  assertEquals(grid.getItemColumn(7), 2);
  assertEquals(grid.getItemIndex(2, 2), 7);
  // Item 8,9 is in group #3/earlier_this_week, row 3.
  assertEquals(grid.getFirstItemInRow(3), 8);
  assertEquals(grid.getItemRow(8), 3);
  assertEquals(grid.getItemColumn(8), 0);
  assertEquals(grid.getItemIndex(3, 0), 8);
  assertEquals(grid.getItemRow(9), 3);
  assertEquals(grid.getItemColumn(9), 1);
  assertEquals(grid.getItemIndex(3, 1), 9);
  // Item 10,11,12 is in group #4/earlier_this_month, row 4.
  assertEquals(grid.getFirstItemInRow(4), 10);
  assertEquals(grid.getItemRow(10), 4);
  assertEquals(grid.getItemColumn(10), 0);
  assertEquals(grid.getItemIndex(4, 0), 10);
  assertEquals(grid.getItemRow(11), 4);
  assertEquals(grid.getItemColumn(11), 1);
  assertEquals(grid.getItemIndex(4, 1), 11);
  assertEquals(grid.getItemRow(12), 4);
  assertEquals(grid.getItemColumn(12), 2);
  assertEquals(grid.getItemIndex(4, 2), 12);
  // Item 13,14,15 is in group #4/earlier_this_month, row 5.
  assertEquals(grid.getFirstItemInRow(5), 13);
  assertEquals(grid.getItemRow(13), 5);
  assertEquals(grid.getItemColumn(13), 0);
  assertEquals(grid.getItemIndex(5, 0), 13);
  assertEquals(grid.getItemRow(14), 5);
  assertEquals(grid.getItemColumn(14), 1);
  assertEquals(grid.getItemIndex(5, 1), 14);
  assertEquals(grid.getItemRow(15), 5);
  assertEquals(grid.getItemColumn(15), 2);
  assertEquals(grid.getItemIndex(5, 2), 15);
  // Item 16 is in group #5/earlier_this_year, row 6.
  assertEquals(grid.getFirstItemInRow(6), 16);
  assertEquals(grid.getItemRow(16), 6);
  assertEquals(grid.getItemColumn(16), 0);
  assertEquals(grid.getItemIndex(6, 0), 16);
  // Item 17,18,19 is in group #6/older, row 7.
  assertEquals(grid.getFirstItemInRow(7), 17);
  assertEquals(grid.getItemRow(17), 7);
  assertEquals(grid.getItemColumn(17), 0);
  assertEquals(grid.getItemIndex(7, 0), 17);
  assertEquals(grid.getItemRow(18), 7);
  assertEquals(grid.getItemColumn(18), 1);
  assertEquals(grid.getItemIndex(7, 1), 18);
  assertEquals(grid.getItemRow(19), 7);
  assertEquals(grid.getItemColumn(19), 2);
  assertEquals(grid.getItemIndex(7, 2), 19);

  // Invalid inputs:
  // row and column is negative.
  assertEquals(grid.getItemIndex(-1, -1), -1);
  assertEquals(grid.getFirstItemInRow(-1), 0);
  // column is too big.
  assertEquals(grid.getItemIndex(0, 10), -1);
  // row is too big.
  assertEquals(grid.getItemIndex(10, 0), -1);
  assertEquals(grid.getFirstItemInRow(10), grid.dataModel.length);
  // column on a specific row is too big for that row.
  assertEquals(grid.getItemIndex(0, 2), -1);
  assertEquals(grid.getItemIndex(6, 2), -1);
  assertEquals(grid.getItemIndex(6, 3), -1);
}

export function testGetAfterFillerHeight() {
  const grid = setupFileGrid();
  grid['getGroupHeadingHeight_'] = () => GROUP_HEADING_HEIGHT;
  const ROW_HEIGHT = FILE_ITEM_HEIGHT;
  assert(grid.dataModel);
  // Enable group by modification time.
  groupByModificationTime(grid.dataModel);
  // Check the comment in groupByModificationTime() for a visual illustration.
  // Note: the index itself is being excluded.
  // Item 0,1 is in group #1/today, 8 rows below + 5 headers.
  assertEquals(
      grid.getAfterFillerHeight(0), 8 * ROW_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(1), 8 * ROW_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(2), 8 * ROW_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
  // Item 2,3,4 is in group #2/yesterday, 7 rows below + 4 header.
  assertEquals(
      grid.getAfterFillerHeight(3), 7 * ROW_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(4), 7 * ROW_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(5), 7 * ROW_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  // Item 5,6,7 is in group #3/earlier_this_week, 6 rows below + 3 headers.
  assertEquals(
      grid.getAfterFillerHeight(6), 6 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(7), 6 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(8), 6 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 8,9 is in group #3/earlier_this_week, 5 rows below + 3 headers.
  assertEquals(
      grid.getAfterFillerHeight(9), 5 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(10), 5 * ROW_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 10,11,12 is in group #4/earlier_this_month, 4 rows below + 2 headers.
  assertEquals(
      grid.getAfterFillerHeight(11), 4 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(12), 4 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(13), 4 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  // Item 13,14,15 is in group #4/earlier_this_month, 3 rows below + 2 headers.
  assertEquals(
      grid.getAfterFillerHeight(14), 3 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(15), 3 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  assertEquals(
      grid.getAfterFillerHeight(16), 3 * ROW_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  // Item 16 is in group #5/earlier_this_year, 2 rows below + 1 header.
  assertEquals(
      grid.getAfterFillerHeight(17), 2 * ROW_HEIGHT + 1 * GROUP_HEADING_HEIGHT);
  // Item 17,18,19 is in group #6/older, 1 row below.
  assertEquals(grid.getAfterFillerHeight(18), 1 * ROW_HEIGHT);
  assertEquals(grid.getAfterFillerHeight(19), 1 * ROW_HEIGHT);
  assertEquals(grid.getAfterFillerHeight(20), 1 * ROW_HEIGHT);
}

export function testGetRowForListOffset() {
  const grid = setupFileGrid();
  grid['getGroupHeadingHeight_'] = () => GROUP_HEADING_HEIGHT;
  grid['paddingTop_'] = 0;  // To ease calculation.
  assert(grid.dataModel);
  // Enable group by modification time.
  groupByModificationTime(grid.dataModel);
  // index                                       height      total height
  // --------------------------------------------------------------------
  // Heading #1/today:                             30              30
  // (row 0)   Item 0    Item 1                    50              80
  // Heading #2/yesterday:                         30              110
  // (row 1)   Item 2    Item 3    Item 4          50              160
  // Heading #3/earlier_this_week:                 30              190
  // (row 2)   Item 5    Item 6    Item 7          50              240
  // (row 3)   Item 8    Item 9                    50              290
  // Heading #4/earlier_this_month:                30              320
  // (row 4)   Item 10   Item 11   Item 12         50              370
  // (row 5)   Item 13   Item 14   Item 15         50              420
  // Heading #5/earlier_this_year:                 30              450
  // (row 6)   Item 16                             50              500
  // Heading #6/older:                             30              530
  // (row 7)   Item 17   Item 18   Item 19         50              580
  assertEquals(grid['getRowForListOffset_'](0), 0);
  assertEquals(grid['getRowForListOffset_'](30), 0);
  assertEquals(grid['getRowForListOffset_'](100), 1);
  assertEquals(grid['getRowForListOffset_'](200), 2);
  assertEquals(grid['getRowForListOffset_'](240), 3);
  assertEquals(grid['getRowForListOffset_'](300), 4);
  assertEquals(grid['getRowForListOffset_'](400), 5);
  assertEquals(grid['getRowForListOffset_'](450), 6);
  assertEquals(grid['getRowForListOffset_'](500), 7);
  assertEquals(grid['getRowForListOffset_'](600), 7);
}

export function testItemHeightForGroupByModificationTime() {
  const grid = setupFileGrid();
  assert(grid.dataModel);
  // Enable group by modification time.
  groupByModificationTime(grid.dataModel);

  // We are testing the logic in getGroupHeadingHeight_(), so we should use
  // the real MODIFICATION_TIME_GROUP_HEADING_HEIGHT(57) and
  // GROUP_MARGIN_TOP(16) from file_grid.
  assertEquals(grid['getGroupHeadingHeight_'](0), 57);
  assertEquals(grid['getGroupHeadingHeight_'](1), 57 + 16);
  for (let i = 0; i < 20; i++) {
    assertEquals(grid['getItemHeightByIndex_'](i), FILE_ITEM_HEIGHT);
  }
}

export function testItemHeightForGroupByDirectory() {
  const grid = setupFileGrid();
  assert(grid.dataModel);
  // Enable group by directory.
  groupByDirectory(grid.dataModel);

  // We are testing the logic in getGroupHeadingHeight_(), so we should use
  // the real DIRECTORY_GROUP_HEADING_HEIGHT(40) and GROUP_MARGIN_TOP(16)
  // from file_grid.
  assertEquals(grid['getGroupHeadingHeight_'](0), 40);
  assertEquals(grid['getGroupHeadingHeight_'](1), 40 + 16);
  for (let i = 0; i < 20; i++) {
    // index 3 is the last item for folders
    assertEquals(
        grid['getItemHeightByIndex_'](i),
        i <= 3 ? FOLDER_ITEM_HEIGHT : FILE_ITEM_HEIGHT);
  }
}

export function testGetHitRowIndex() {
  const grid = setupFileGrid();
  assert(grid.dataModel);
  // Enable group by directory.
  groupByDirectory(grid.dataModel);
  grid['getGroupHeadingHeight_'] = () => GROUP_HEADING_HEIGHT;

  // index                                       height      total height
  // --------------------------------------------------------------------
  // Heading #1/folders:                          30             30
  // (row 0)    Item 0    Item 1    Item 2        20             50
  // (row 1)    Item 3                            20             70
  // Heading #2/files:                            30             100
  // (row 2)    Item 4    Item 5    Item 6        50             150
  // (row 3)    Item 7    Item 8    Item 9        50             200
  // (row 4)    Item 10   Item 11   Item 12       50             250
  // (row 5)    Item 13   Item 14   Item 15       50             300
  // (row 6)    Item 16   Item 17   Item 18       50             350
  // (row 7)    Item 19                           50             400
  assertEquals(grid['getHitRowIndex_'](0, true), 0);
  assertEquals(grid['getHitRowIndex_'](0, false), -1);
  assertEquals(grid['getHitRowIndex_'](30, true), 0);
  assertEquals(grid['getHitRowIndex_'](30, false), -1);
  assertEquals(grid['getHitRowIndex_'](70, true), 2);
  assertEquals(grid['getHitRowIndex_'](70, false), 1);
  assertEquals(grid['getHitRowIndex_'](70 + ITEM_MARGIN_TOP, true), 2);
  assertEquals(grid['getHitRowIndex_'](70 + ITEM_MARGIN_TOP, false), 1);
  assertEquals(grid['getHitRowIndex_'](100, true), 2);
  assertEquals(grid['getHitRowIndex_'](100, false), 1);
  assertEquals(grid['getHitRowIndex_'](180, true), 3);
  assertEquals(grid['getHitRowIndex_'](180, false), 3);
  assertEquals(grid['getHitRowIndex_'](250 + ITEM_MARGIN_TOP - 1, true), 5);
  assertEquals(grid['getHitRowIndex_'](250 + ITEM_MARGIN_TOP - 1, false), 4);
  // For larger y, the out of bound row index will be returned (e.g. max
  // index = 7), which is expected.
  assertEquals(grid['getHitRowIndex_'](450, true), 8);
  assertEquals(grid['getHitRowIndex_'](450, false), 8);
}

export function testGetHitColumnIndex() {
  const grid = setupFileGrid();
  assert(grid.dataModel);
  // Enable group by directory.
  groupByDirectory(grid.dataModel);
  grid['getGroupHeadingHeight_'] = () => GROUP_HEADING_HEIGHT;

  //           (col 0)  (col 1)   (col 2)
  // -----------------------------------------
  //         |   100 | |  100  | |   100 |
  // -----------------------------------------
  // Heading #1/folders:
  // (row 0)    Item 0    Item 1    Item 2
  // (row 1)    Item 3
  // Heading #2/files:
  // (row 2)    Item 4    Item 5    Item 6
  // (row 3)    Item 7    Item 8    Item 9
  // (row 4)    Item 10   Item 11   Item 12
  // (row 5)    Item 13   Item 14   Item 15
  // (row 6)    Item 16   Item 17   Item 18
  // (row 7)    Item 19
  assertEquals(grid['getHitColumnIndex_'](0, true), 0);
  assertEquals(grid['getHitColumnIndex_'](0, false), -1);
  assertEquals(grid['getHitColumnIndex_'](60, true), 0);
  assertEquals(grid['getHitColumnIndex_'](60, false), 0);
  assertEquals(grid['getHitColumnIndex_'](100 + ITEM_MARGIN_LEFT - 1, true), 1);
  assertEquals(
      grid['getHitColumnIndex_'](100 + ITEM_MARGIN_LEFT - 1, false), 0);
  // For larger x, the out of bound column index will be returned (e.g. max
  // index = 2), which is expected.
  assertEquals(grid['getHitColumnIndex_'](400, true), 4);
  assertEquals(grid['getHitColumnIndex_'](400, false), 3);
}

// Test FileGridSelectionController's getIndexAbove() and getIndexBelow().
export function testSelectionModelIndexMovement() {
  const grid = setupFileGrid();
  assert(grid.dataModel);
  groupByDirectory(grid.dataModel);
  const sm = new FileGridSelectionController(
      new ListSelectionModel(grid.dataModel.length), grid);
  // Heading #1/folders:
  // (row 0)    Item 0    Item 1    Item 2
  // (row 1)    Item 3
  // Heading #2/files:
  // (row 2)    Item 4    Item 5    Item 6
  // (row 3)    Item 7    Item 8    Item 9
  // (row 4)    Item 10   Item 11   Item 12
  // (row 5)    Item 13   Item 14   Item 15
  // (row 6)    Item 16   Item 17   Item 18
  // (row 7)    Item 19

  // getIndexAbove()
  assertEquals(sm.getIndexAbove(0), -1);
  assertEquals(sm.getIndexAbove(1), 0);
  assertEquals(sm.getIndexAbove(2), 0);
  assertEquals(sm.getIndexAbove(3), 0);
  assertEquals(sm.getIndexAbove(4), 3);
  // The col above item 5/6 doesn't have items, so fall back to 3.
  assertEquals(sm.getIndexAbove(5), 3);
  assertEquals(sm.getIndexAbove(6), 3);
  assertEquals(sm.getIndexAbove(11), 8);
  assertEquals(sm.getIndexAbove(15), 12);
  assertEquals(sm.getIndexAbove(19), 16);
  // getIndexBelow()
  assertEquals(sm.getIndexBelow(0), 3);
  // The col below item 1/2 doesn't have items, so fall back to 3.
  assertEquals(sm.getIndexBelow(1), 3);
  assertEquals(sm.getIndexBelow(2), 3);
  assertEquals(sm.getIndexBelow(3), 4);
  assertEquals(sm.getIndexBelow(4), 7);
  assertEquals(sm.getIndexBelow(8), 11);
  assertEquals(sm.getIndexBelow(12), 15);
  // The col below item 17/18 doesn't have items, so fall back to 19.
  assertEquals(sm.getIndexBelow(17), 19);
  assertEquals(sm.getIndexBelow(18), 19);
  assertEquals(sm.getIndexBelow(19), -1);
}
