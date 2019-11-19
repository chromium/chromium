// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @type {!MockVolumeManager} */
let volumeManager;

/** @type {!DirectoryModel} */
let directoryModel;

/** @type {!MetadataModel} */
let metadataModel;

/** @type {!importer.HistoryLoader} */
let historyLoader;

/** @type {!HTMLElement} */
let element;

/** @type {!A11yAnnounce} */
let a11y;

// Set up test components.
function setUp() {
  // Mock LoadTimeData strings.
  window.loadTimeData.getString = id => id;
  window.loadTimeData.data = {};

  // Setup mock components.
  volumeManager = new MockVolumeManager();
  metadataModel = new MockMetadataModel({});
  historyLoader = /** @type {!importer.HistoryLoader} */ ({
    getHistory: () => {
      return Promise.resolve();
    },
  });

  const a11Messages = [];
  a11y = /** @type {!A11yAnnounce} */ ({
    speakA11yMessage: (text) => {
      a11Messages.push(text);
    },
  });

  // Create DOM element parent of the file list under test.
  element = setupBody();
}

/**
 * Returns the element used to parent the file list. The element is
 * attached to the body, and styled for visual display.
 *
 * @return {!HTMLElement}
 */
function setupBody() {
  const style = `
      <style>
        list {
          display: block;
          height: 200px;
          width: 800px;
        }
       </style>
      `;
  document.body.innerHTML = style;

  const element = document.createElement('div');
  document.body.appendChild(element);
  return /** @type {!HTMLElement} */ (element);
}

/** @param {string} keyName */
function key(keyName) {
  return {
    bubbles: true,
    composed: true,
    key: keyName,
  };
}

/**
 * @param {string} keyName
 * @param {string=} code event.code value.
 */
function ctrlAndKey(keyName, code) {
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
function testMultipleSelectionWithKeyboard() {
  // Render the FileTable on |element|.
  const fullPage = true;
  FileTable.decorate(
      element, metadataModel, volumeManager, historyLoader, a11y, fullPage);

  // Overwrite the selectionModel of the FileTable class (since events
  // would be handled by cr.ui.ListSelectionModel otherwise).
  const sm = new FileListSelectionModel();
  const table = /** @type {FileTable} */ (element);
  table.selectionModel = sm;

  // Add FileTableList file entries, then draw and focus the table list.
  const entries = [
    new FakeEntry('entry1-label', VolumeManagerCommon.RootType.CROSTINI),
    new FakeEntry('entry2-label', VolumeManagerCommon.RootType.CROSTINI),
    new FakeEntry('entry3-label', VolumeManagerCommon.RootType.CROSTINI),
  ];
  const dataModel = new FileListModel(metadataModel);
  dataModel.splice(0, 0, ...entries);
  const tableList = /** @type {FileTableList} */ (element.list);
  tableList.dataModel = dataModel;
  tableList.redraw();
  tableList.focus();

  // Grab all the elements in the file list.
  const listItem0 = tableList.items[0];
  const listItem1 = tableList.items[1];
  const listItem2 = tableList.items[2];

  // Assert file table list |item| selection state.
  function assertItemIsSelected(item, selected = true) {
    if (selected) {
      assertTrue(item.hasAttribute('selected'));
      assertEquals('true', item.getAttribute('aria-selected'));
    } else {
      assertFalse(item.hasAttribute('selected'));
      assertEquals(null, item.getAttribute('aria-selected'));
    }
  }

  // Assert file table list |item| focus/lead state.
  function assertItemIsTheLead(item, lead = true) {
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
          tableList.items[i].hasAttribute('lead'),
          'item ' + i + ' should not have focus');
    }
    assertEquals(
        'false', tableList.items[i].getAttribute('aria-selected'),
        'item ' + i + ' should have aria-selected=false');
    assertFalse(
        tableList.items[i].hasAttribute('selected'),
        'item ' + i + ' should not have selected attr');
  }
}

function testKeyboardOperations() {
  // Render the FileTable on |element|.
  const fullPage = true;
  FileTable.decorate(
      element, metadataModel, volumeManager, historyLoader, a11y, fullPage);

  // Overwrite the selectionModel of the FileTable class (since events
  // would be handled by cr.ui.ListSelectionModel otherwise).
  const sm = new FileListSelectionModel();
  const table = /** @type {FileTable} */ (element);
  table.selectionModel = sm;

  // Add FileTableList file entries, then draw and focus the table list.
  const entries = [
    new FakeEntry('entry1-label', VolumeManagerCommon.RootType.CROSTINI),
    new FakeEntry('entry2-label', VolumeManagerCommon.RootType.CROSTINI),
    new FakeEntry('entry3-label', VolumeManagerCommon.RootType.CROSTINI),
  ];
  const dataModel = new FileListModel(metadataModel);
  dataModel.splice(0, 0, ...entries);
  const tableList = /** @type {FileTableList} */ (element.list);
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
