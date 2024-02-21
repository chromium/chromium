// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../../background/js/mock_volume_manager.js';
import {FakeEntryImpl} from '../../../common/js/files_app_entry_types.js';
import {RootType} from '../../../common/js/volume_manager_types.js';
import {FileListModel} from '../file_list_model.js';
import type {MetadataModel} from '../metadata/metadata_model.js';
import {MockMetadataModel} from '../metadata/mock_metadata.js';

import type {A11yAnnounce} from './a11y_announce.js';
import {FileListSelectionModel} from './file_list_selection_model.js';
import {FileTable} from './file_table.js';
import type {FileTableList} from './file_table_list.js';

let volumeManager: MockVolumeManager;

let metadataModel: MetadataModel;

let element: HTMLElement;

let a11y: A11yAnnounce;

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

  // Create DOM element parent of the file list under test.
  element = setupBody();
}

/**
 * Returns the element used to parent the file list. The element is
 * attached to the body, and styled for visual display.
 *
 */
function setupBody(): HTMLElement {
  document.body.innerHTML = getTrustedHTML`
    <style>
      list {
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

function key(keyName: string) {
  return {
    bubbles: true,
    composed: true,
    key: keyName,
  };
}

/**
 * @param code event.code value.
 */
function ctrlAndKey(keyName: string, code?: string) {
  return {
    ctrlKey: true,
    shiftKey: false,
    altKey: false,
    bubbles: true,
    composed: true,
    key: keyName,
    code: code,
    // Get keyCode for key like A, B but not for Escape, Arrow, etc.
    // A==65, B==66, etc.
    keyCode: (keyName && keyName.length === 1) ? keyName.charCodeAt(0) :
                                                 undefined,
  };
}

/**
 * Tests that the keyboard can be used to navigate the FileTableList.
 */
export function testMultipleSelectionWithKeyboard() {
  // Render the FileTable on |element|.
  const fullPage = true;
  FileTable.decorate(element, metadataModel, volumeManager, a11y, fullPage);

  // Overwrite the selectionModel of the FileTable class (since events
  // would be handled by cr.ui.ListSelectionModel otherwise).
  const sm = new FileListSelectionModel();
  const table = element as unknown as FileTable;
  table.selectionModel = sm;

  // Add FileTableList file entries, then draw and focus the table list.
  const entries = [
    new FakeEntryImpl('entry1-label', RootType.CROSTINI),
    new FakeEntryImpl('entry2-label', RootType.CROSTINI),
    new FakeEntryImpl('entry3-label', RootType.CROSTINI),
  ];
  const dataModel = new FileListModel(metadataModel);
  dataModel.splice(0, 0, ...entries);
  const tableList = table.list;
  tableList.dataModel = dataModel;
  tableList.redraw();
  tableList.focus();

  // Grab all the elements in the file list.
  const listItem0 = tableList.items[0]!;
  const listItem1 = tableList.items[1]!;
  const listItem2 = tableList.items[2]!;

  // Assert file table list |item| selection state.
  function assertItemIsSelected(item: HTMLElement, selected = true) {
    if (selected) {
      assertTrue(item.hasAttribute('selected'));
      assertEquals('true', item.getAttribute('aria-selected'));
    } else {
      assertFalse(item.hasAttribute('selected'));
      assertEquals(null, item.getAttribute('aria-selected'));
    }
  }

  // Assert file table list |item| focus/lead state.
  function assertItemIsTheLead(item: HTMLElement, lead = true) {
    if (lead) {
      assertEquals('lead', item.getAttribute('lead'));
      assertEquals(item.id, tableList.getAttribute('aria-activedescendant'));
    } else {
      assertFalse(item.hasAttribute('lead'));
      assertFalse(item.id === tableList.getAttribute('aria-activedescendant'));
    }
  }

  // FileTableList always allows multiple selection.
  assertEquals('true', tableList.getAttribute('aria-multiselectable'));

  // Home key selects the first item (listItem0).
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('Home')));
  // Only 1 item selected.
  assertEquals(1, sm.selectedIndexes.length);
  // listItem0 should be selected and focused.
  assertEquals(0, sm.selectedIndexes[0]);
  assertItemIsTheLead(listItem0);
  assertItemIsSelected(listItem0);
  // Only one item is selected: multiple selection should be inactive.
  assertFalse(sm.getCheckSelectMode());

  // ArrowDown moves and selects next item.
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('ArrowDown')));
  // Only listItem1 should be selected.
  assertEquals(1, sm.selectedIndexes.length);
  assertEquals(1, sm.selectedIndexes[0]);
  // listItem1 should be focused.
  assertItemIsTheLead(listItem1);
  assertItemIsSelected(listItem1);
  // Only one item is selected: multiple selection should be inactive.
  assertFalse(sm.getCheckSelectMode());

  // Ctrl+ArrowDown only moves the focus.
  tableList.dispatchEvent(
      new KeyboardEvent('keydown', ctrlAndKey('ArrowDown')));
  // listItem1 should be not focused but still selected.
  assertEquals(1, sm.selectedIndexes.length);
  assertEquals(1, sm.selectedIndexes[0]);
  assertItemIsTheLead(listItem1, false);
  assertItemIsSelected(listItem1);
  // listItem2 should be focused but not selected.
  assertItemIsTheLead(listItem2);
  assertItemIsSelected(listItem2, false);

  // Only one item is selected: multiple selection should be inactive.
  assertFalse(sm.getCheckSelectMode());

  // Ctrl+Space selects the focused item.
  tableList.dispatchEvent(
      new KeyboardEvent('keydown', ctrlAndKey(' ', 'Space')));
  // Multiple selection mode should now be activated.
  assertTrue(sm.getCheckSelectMode());
  // Both listItem1 and listItem2 should be selected.
  assertEquals(2, sm.selectedIndexes.length);
  assertEquals(1, sm.selectedIndexes[0]);
  assertEquals(2, sm.selectedIndexes[1]);
  // listItem1 should not be focused.
  assertItemIsTheLead(listItem1, false);
  assertItemIsSelected(listItem1);
  // listItem1 should be focused and selected.
  assertItemIsTheLead(listItem2);
  assertItemIsSelected(listItem2);

  // Hit Esc to cancel the whole selection.
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('Escape')));
  // The item with the focus should not change.
  assertItemIsTheLead(listItem2);
  // But there should be no selected items anymore.
  assertFalse(sm.getCheckSelectMode());
  assertEquals(0, sm.selectedIndexes.length);
  for (let i = 0; i < tableList.items.length; i++) {
    if (i !== 2) {
      // Item 2 should have focus.
      assertFalse(
          tableList.items[i]!.hasAttribute('lead'),
          'item ' + i + ' should not have focus');
    }
    assertEquals(
        'false', tableList.items[i]!.getAttribute('aria-selected'),
        'item ' + i + ' should have aria-selected=false');
    assertFalse(
        tableList.items[i]!.hasAttribute('selected'),
        'item ' + i + ' should not have selected attr');
  }
}

export function testKeyboardOperations() {
  // Render the FileTable on |element|.
  const fullPage = true;
  FileTable.decorate(element, metadataModel, volumeManager, a11y, fullPage);

  // Overwrite the selectionModel of the FileTable class (since events
  // would be handled by cr.ui.ListSelectionModel otherwise).
  const sm = new FileListSelectionModel();
  const table = element as unknown as FileTable;
  table.selectionModel = sm;

  // Add FileTableList file entries, then draw and focus the table list.
  const entries = [
    new FakeEntryImpl('entry1-label', RootType.CROSTINI),
    new FakeEntryImpl('entry2-label', RootType.CROSTINI),
    new FakeEntryImpl('entry3-label', RootType.CROSTINI),
  ];
  const dataModel = new FileListModel(metadataModel);
  dataModel.splice(0, 0, ...entries);
  const tableList = table.list;
  tableList.dataModel = dataModel;
  tableList.redraw();
  tableList.focus();

  // Home key selects the first item (index 0).
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('Home')));
  // Only 1 item selected.
  assertEquals(1, sm.selectedIndexes.length);
  // Index 0 should be selected and focused.
  assertEquals(0, sm.selectedIndexes[0]);

  // End key selects the last item (index 2).
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('End')));
  // Only 1 item selected.
  assertEquals(1, sm.selectedIndexes.length);
  // Index 2 should be selected and focused.
  assertEquals(2, sm.selectedIndexes[0]);

  // Ctrl+A key selects all items.
  tableList.dispatchEvent(new KeyboardEvent('keydown', ctrlAndKey('A')));
  // All 3 items are selected.
  assertEquals(3, sm.selectedIndexes.length);
  assertEquals(0, sm.selectedIndexes[0]);
  assertEquals(1, sm.selectedIndexes[1]);
  assertEquals(2, sm.selectedIndexes[2]);

  // Escape key selects all items.
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('Escape')));
  // All 3 items are selected.
  assertEquals(0, sm.selectedIndexes.length);

  // Home key selects the first item (index 0).
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('Home')));
  assertEquals(1, sm.selectedIndexes.length);
  assertEquals(0, sm.selectedIndexes[0]);

  // ArrowDown moves and selects next item.
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('ArrowDown')));
  // Only index 1 should be selected.
  assertEquals(1, sm.selectedIndexes.length);
  assertEquals(1, sm.selectedIndexes[0]);

  // ArrowUp moves and selects previous item.
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('ArrowUp')));
  // Only index 0 should be selected.
  assertEquals(1, sm.selectedIndexes.length);
  assertEquals(0, sm.selectedIndexes[0]);

  // ArrowLeft and ArrowRight aren't really implemented.
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('ArrowLeft')));
  // Selected item remains the same.
  assertEquals(1, sm.selectedIndexes.length);
  assertEquals(0, sm.selectedIndexes[0]);
  tableList.dispatchEvent(new KeyboardEvent('keydown', key('ArrowRight')));
  assertEquals(1, sm.selectedIndexes.length);
  assertEquals(0, sm.selectedIndexes[0]);
}


// Force round number heights to simplify the math in the test.
const ITEM_HEIGHT = 40;
const GROUP_HEADING_HEIGHT = 20;

function setupFileTableList(): FileTableList {
  FileTable.decorate(element, metadataModel, volumeManager, a11y, true);
  const table = element as unknown as FileTable;

  // Add 10 fake files.
  const entries = [];
  for (let i = 1; i <= 10; i++) {
    entries.push(new FakeEntryImpl(`${i}.txt`, RootType.RECENT));
  }
  const dataModel = new FileListModel(metadataModel);
  // Disable group by.
  dataModel.shouldShowGroupHeading = () => false;
  dataModel.splice(0, 0, ...entries);
  const tableList = table.list as FileTableList;
  tableList.dataModel = dataModel;
  // Mock item size.
  tableList['getDefaultItemHeight_'] = () => ITEM_HEIGHT;
  tableList['getGroupHeadingHeight_'] = () => GROUP_HEADING_HEIGHT;
  return tableList;
}

/**
 */
function enableGroupByForDataModel(fileListModel: FileListModel) {
  const RecentDateBucket = chrome.fileManagerPrivate.RecentDateBucket;

  // Mock group by information.
  fileListModel.shouldShowGroupHeading = () => true;
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
        endIndex: 2,
        label: 'yesterday',
        group: RecentDateBucket.YESTERDAY,
      },
      {
        startIndex: 3,
        endIndex: 4,
        label: 'earlier_this_week',
        group: RecentDateBucket.EARLIER_THIS_WEEK,
      },
      {
        startIndex: 5,
        endIndex: 6,
        label: 'earlier_this_month',
        group: RecentDateBucket.EARLIER_THIS_MONTH,
      },
      {
        startIndex: 7,
        endIndex: 8,
        label: 'earlier_this_year',
        group: RecentDateBucket.EARLIER_THIS_YEAR,
      },
      {
        startIndex: 9,
        endIndex: 9,
        label: 'older',
        group: RecentDateBucket.OLDER,
      },
    ];
  };
}

export function testGetItemTop() {
  const tableList = setupFileTableList();
  // No group heading, so only the item height is used.
  const len = tableList.dataModel?.length ?? 0;
  for (let i = 0; i < len; i++) {
    assertEquals(tableList.getItemTop(i), i * ITEM_HEIGHT);
  }

  // Enable group by.
  enableGroupByForDataModel(tableList.dataModel);
  // Item 0 is in group #1/today, nothing is above it.
  assertEquals(tableList.getItemTop(0), 0);
  // Item 1 is in group #1/today, 1 item above + 1 header.
  assertEquals(tableList.getItemTop(1), 1 * ITEM_HEIGHT + GROUP_HEADING_HEIGHT);
  // Item 2 is in group #2/yesterday, 2 items above + 1 header.
  assertEquals(tableList.getItemTop(2), 2 * ITEM_HEIGHT + GROUP_HEADING_HEIGHT);
  // Item 3 is in group #3/earlier_this_week, 3 items above + 2 headers.
  assertEquals(
      tableList.getItemTop(3), 3 * ITEM_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  // Item 4 is in group #3/earlier_this_week, 4 items above + 3 headers.
  assertEquals(
      tableList.getItemTop(4), 4 * ITEM_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 5 is in group #4/earlier_this_month, 5 items above + 3 headers.
  assertEquals(
      tableList.getItemTop(5), 5 * ITEM_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 6 is in group #4/earlier_this_month, 6 items above + 4 headers.
  assertEquals(
      tableList.getItemTop(6), 6 * ITEM_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  // Item 7 is in group #5/earlier_this_year, 7 items above + 4 headers.
  assertEquals(
      tableList.getItemTop(7), 7 * ITEM_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  // Item 8 is in group #5/earlier_this_year, 8 items above + 5 headers.
  assertEquals(
      tableList.getItemTop(8), 8 * ITEM_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
  // Item 9 is in group #6/older, 9 items above + 5 headers.
  assertEquals(
      tableList.getItemTop(9), 9 * ITEM_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
}

export function testGetAfterFillerHeight() {
  const tableList = setupFileTableList();

  // No group heading, so only the item height is used.
  const totalLength = tableList.dataModel?.length ?? 0;
  for (let i = 0; i < totalLength; i++) {
    assertEquals(
        tableList.getAfterFillerHeight(i),
        i === 0 ? 1 : (totalLength - i) * ITEM_HEIGHT);
  }

  // Enable group by.
  enableGroupByForDataModel(tableList.dataModel);
  // A special case handled in file_table.js.
  assertEquals(tableList.getAfterFillerHeight(0), 1);
  // Item 1 is in group #1/today, 9 items below + 5 headers.
  assertEquals(
      tableList.getAfterFillerHeight(1),
      9 * ITEM_HEIGHT + 5 * GROUP_HEADING_HEIGHT);
  // Item 1 is in group #2/yesterday, 8 items below + 4 headers.
  assertEquals(
      tableList.getAfterFillerHeight(2),
      8 * ITEM_HEIGHT + 4 * GROUP_HEADING_HEIGHT);
  // Item 1 is in group #3/earlier_this_week, 7 items below + 3 headers.
  assertEquals(
      tableList.getAfterFillerHeight(3),
      7 * ITEM_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 1 is in group #3/earlier_this_week, 6 items below + 3 headers.
  assertEquals(
      tableList.getAfterFillerHeight(4),
      6 * ITEM_HEIGHT + 3 * GROUP_HEADING_HEIGHT);
  // Item 1 is in group #4/earlier_this_month, 5 items below + 2 headers.
  assertEquals(
      tableList.getAfterFillerHeight(5),
      5 * ITEM_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  // Item 1 is in group #4/earlier_this_month, 4 items below + 2 headers.
  assertEquals(
      tableList.getAfterFillerHeight(6),
      4 * ITEM_HEIGHT + 2 * GROUP_HEADING_HEIGHT);
  // Item 7 is in group #5/earlier_this_year, 3 items below + 1 header.
  assertEquals(
      tableList.getAfterFillerHeight(7),
      3 * ITEM_HEIGHT + 1 * GROUP_HEADING_HEIGHT);
  // Item 8 is in group #5/earlier_this_year, 2 items below + 1 header.
  assertEquals(
      tableList.getAfterFillerHeight(8),
      2 * ITEM_HEIGHT + 1 * GROUP_HEADING_HEIGHT);
  // Item 9 is in group #6/older, 1 item below.
  assertEquals(tableList.getAfterFillerHeight(9), 1 * ITEM_HEIGHT);
}

export function testGetIndexForListOffset() {
  const tableList = setupFileTableList();

  // No group heading.
  // index      height      total height
  // -----------------------------------
  // Item 0       40            40
  // Item 1       40            80
  // Item 2       40            120
  // Item 3       40            160
  // Item 4       40            200
  // Item 5       40            240
  // Item 6       40            280
  // Item 7       40            320
  // Item 8       40            360
  // Item 9       40            400
  assertEquals(tableList['getIndexForListOffset_'](0), 0);
  assertEquals(tableList['getIndexForListOffset_'](40), 1);
  assertEquals(tableList['getIndexForListOffset_'](100), 2);
  assertEquals(tableList['getIndexForListOffset_'](200), 5);
  assertEquals(tableList['getIndexForListOffset_'](240), 6);
  assertEquals(tableList['getIndexForListOffset_'](300), 7);
  // Note: The returned index could be an invalid array index, which is
  // expected e.g. 10 here is larger than the largest index 9 in the array.
  assertEquals(tableList['getIndexForListOffset_'](400), 10);

  // Enable group by.
  enableGroupByForDataModel(tableList.dataModel);
  // index      height      total height
  // -----------------------------------
  // Heading 1    20            20
  // Item 0       40            60
  // Item 1       40            100
  // Heading 2    20            120
  // Item 2       40            160
  // Heading 3    20            180
  // Item 3       40            220
  // Item 4       40            260
  // Heading 4    20            280
  // Item 5       40            320
  // Item 6       40            360
  // Heading 5    20            380
  // Item 7       40            420
  // Item 8       40            460
  // Heading 6    20            480
  // Item 9       40            520
  assertEquals(tableList['getIndexForListOffset_'](0), 0);
  assertEquals(tableList['getIndexForListOffset_'](40), 0);
  assertEquals(tableList['getIndexForListOffset_'](100), 2);
  assertEquals(tableList['getIndexForListOffset_'](200), 3);
  assertEquals(tableList['getIndexForListOffset_'](240), 4);
  assertEquals(tableList['getIndexForListOffset_'](300), 5);
  assertEquals(tableList['getIndexForListOffset_'](400), 7);
  assertEquals(tableList['getIndexForListOffset_'](500), 9);
}

export function testGetHitElements() {
  const tableList = setupFileTableList();

  // No group heading.
  // index      height      total height
  // -----------------------------------
  // Item 0       40            40
  // Item 1       40            80
  // Item 2       40            120
  // Item 3       40            160
  // Item 4       40            200
  // Item 5       40            240
  // Item 6       40            280
  // Item 7       40            320
  // Item 8       40            360
  // Item 9       40            400

  // Passing -1 for 1st/3rd parameter because we don't care the x coordinates
  // and the width of the drag selection.
  assertArrayEquals(tableList.getHitElements(-1, 10), [0]);
  assertArrayEquals(
      tableList.getHitElements(-1, 50, -1, 200), [1, 2, 3, 4, 5, 6]);
  assertArrayEquals(tableList.getHitElements(-1, 240, -1, 100), [5, 6, 7, 8]);

  // Enable group by.
  enableGroupByForDataModel(tableList.dataModel);
  // index      height      total height
  // -----------------------------------
  // Heading 1    20            20
  // Item 0       40            60
  // Item 1       40            100
  // Heading 2    20            120
  // Item 2       40            160
  // Heading 3    20            180
  // Item 3       40            220
  // Item 4       40            260
  // Heading 4    20            280
  // Item 5       40            320
  // Item 6       40            360
  // Heading 5    20            380
  // Item 7       40            420
  // Item 8       40            460
  // Heading 6    20            480
  // Item 9       40            520

  // Passing -1 for 1st/3rd parameter because we don't care the x coordinates
  // and the width of the drag selection.
  assertArrayEquals(tableList.getHitElements(-1, 10), []);
  assertArrayEquals(tableList.getHitElements(-1, 40), [0]);
  assertArrayEquals(tableList.getHitElements(-1, 50, -1, 200), [0, 1, 2, 3, 4]);
  assertArrayEquals(tableList.getHitElements(-1, 220, -1, 100), [3, 4, 5]);
}
