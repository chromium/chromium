/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.ui.SelectionModelTest');
goog.setTestOnly();

const SelectionModel = goog.require('goog.ui.SelectionModel');
const dispose = goog.require('goog.dispose');
const googArray = goog.require('goog.array');
const recordFunction = goog.require('goog.testing.recordFunction');
const testSuite = goog.require('goog.testing.testSuite');

let addedItem;
let addedItems;
let items;
let selectionModel;

/*
 * Checks that the selection model returns the correct item count.
 */

/*
 * Checks that the correct first element is returned by the selection model.
 */

/*
 * Checks that the correct last element is returned by the selection model.
 */

/*
 * Tests the behavior of goog.ui.SelectionModel.getItemAt(index).
 */

/*
 * Checks that an item can be correctly added to the selection model.
 */

/*
 * Checks that an item can be added to the selection model at a specific index.
 */

/*
 * Checks that multiple items can be correctly added to the selection model.
 */

/*
 * Checks that all elements can be removed from the selection model.
 */

/*
 * Checks that all items can be obtained from the selection model.
 */

/*
 * Checks that an item's index can be found in the selection model.
 */

/*
 * Checks that an item can be removed from the selection model.
 */

/*
 * Checks that an item at a particular index can be removed from the selection
 * model.
 */

/*
 * Checks that item selection at a particular index works.
 */

/*
 * Checks that items can be selected in the selection model.
 */

/*
 * Checks that an installed handler is called on selection change.
 */

testSuite({
  setUp() {
    items = [1, 2, 3, 4];
    addedItem = 5;
    addedItems = [6, 7, 8];
    selectionModel = new SelectionModel(items);
  },

  tearDown() {
    dispose(selectionModel);
  },

  testGetItemCount() {
    assertEquals(items.length, selectionModel.getItemCount());
  },

  testGetFirst() {
    assertEquals(items[0], selectionModel.getFirst());
  },

  testGetLast() {
    assertEquals(items[items.length - 1], selectionModel.getLast());
  },

  testGetItemAt() {
    items.forEach((item, i) => {
      assertEquals(item, selectionModel.getItemAt(i));
    });
  },

  testAddItem() {
    assertEquals(items.length, selectionModel.getItemCount());

    selectionModel.addItem(addedItem);

    assertEquals(items.length + 1, selectionModel.getItemCount());
    assertEquals(addedItem, selectionModel.getLast());
  },

  testAddItemAt() {
    assertEquals(items.length, selectionModel.getItemCount());

    const insertIndex = 2;
    assertEquals(items[insertIndex], selectionModel.getItemAt(insertIndex));

    selectionModel.addItemAt(addedItem, insertIndex);

    const resultArray = googArray.clone(items);
    googArray.insertAt(resultArray, addedItem, insertIndex);

    assertEquals(items.length + 1, selectionModel.getItemCount());
    assertEquals(addedItem, selectionModel.getItemAt(insertIndex));
    assertArrayEquals(resultArray, selectionModel.getItems());
  },

  testAddItems() {
    assertEquals(items.length, selectionModel.getItemCount());

    selectionModel.addItems(addedItems);

    assertEquals(
        items.length + addedItems.length, selectionModel.getItemCount());

    const resultArray = items.concat(addedItems);
    assertArrayEquals(resultArray, selectionModel.getItems());
  },

  testClear() {
    assertArrayEquals(items, selectionModel.getItems());

    selectionModel.clear();

    assertArrayEquals([], selectionModel.getItems());
  },

  testGetItems() {
    assertArrayEquals(items, selectionModel.getItems());
  },

  testIndexOfItem() {
    items.forEach((item, i) => {
      assertEquals(i, selectionModel.indexOfItem(item));
    });
  },

  testRemoveItem() {
    assertEquals(items.length, selectionModel.getItemCount());

    const resultArray = googArray.clone(items);
    googArray.removeAt(resultArray, 2);

    selectionModel.removeItem(items[2]);

    assertEquals(items.length - 1, selectionModel.getItemCount());
    assertArrayEquals(resultArray, selectionModel.getItems());
  },

  testRemoveItemAt() {
    assertEquals(items.length, selectionModel.getItemCount());

    const resultArray = googArray.clone(items);
    const removeIndex = 2;

    googArray.removeAt(resultArray, removeIndex);

    selectionModel.removeItemAt(removeIndex);

    assertEquals(items.length - 1, selectionModel.getItemCount());
    assertNotEquals(items[removeIndex], selectionModel.getItemAt(removeIndex));
    assertArrayEquals(resultArray, selectionModel.getItems());
  },

  testSelectedIndex() {
    // Default selected index is -1
    assertEquals(-1, selectionModel.getSelectedIndex());

    selectionModel.setSelectedIndex(2);

    assertEquals(2, selectionModel.getSelectedIndex());
    assertEquals(items[2], selectionModel.getSelectedItem());
  },

  testSelectedItem() {
    assertNull(selectionModel.getSelectedItem());

    selectionModel.setSelectedItem(items[1]);

    assertNotNull(selectionModel.getSelectedItem());
    assertEquals(items[1], selectionModel.getSelectedItem());
    assertEquals(1, selectionModel.getSelectedIndex());
  },

  testSelectionHandler() {
    /** @suppress {checkTypes} suppression added to enable type checking */
    const myRecordFunction = new recordFunction();

    selectionModel.setSelectionHandler(myRecordFunction);

    // Select index 2
    selectionModel.setSelectedIndex(2);
    // De-select 2 and select 3
    selectionModel.setSelectedIndex(3);

    const recordCalls = myRecordFunction.getCalls();

    assertEquals(3, recordCalls.length);
    // Calls: Select items[2], de-select items[2], select items[3]
    assertArrayEquals([items[2], true], recordCalls[0].getArguments());
    assertArrayEquals([items[2], false], recordCalls[1].getArguments());
    assertArrayEquals([items[3], true], recordCalls[2].getArguments());
  },
});
