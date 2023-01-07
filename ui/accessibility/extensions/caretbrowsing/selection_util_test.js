// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['selection_util.js', 'traverse_util.js']);

GEN_INCLUDE(['../testing/webstore_extension_test_base.js']);

/** Test fixture for selection_util.js. */
SelectionUtilUnitTest = class extends WebstoreExtensionTest {};

/**
 * @param {chrome.automation.Rect} expected
 * @param {chrome.automation.Rect} actual
 * @return {boolean}
 */
function checkRect(expected, actual) {
  assertEquals(expected.left, actual.left);
  assertEquals(expected.top, actual.top);
  assertEquals(expected.width, actual.width);
  assertEquals(expected.height, actual.height);
}

/**
 * @param {Cursor} expected
 * @param {Cursor} actual
 * @return {boolean}
 */
function checkCursor(expected, actual) {
  assertEquals(expected.node, actual.node);
  assertEquals(expected.index, actual.index);
  assertEquals(expected.text, actual.text);
}

TEST_F('SelectionUtilUnitTest', 'GetCursorRect', function() {
  const text = document.createTextNode('My Favorite Things');
  const div = document.createElement('div');
  div.appendChild(text);
  document.body.appendChild(div);
  div.style = 'position: absolute; left: 100px; top: 70px';

  let cursor = new Cursor(text, 0, text.nodeValue);
  let expected = {left: 100, top: 70, width: 1, height: 19};
  let actual = SelectionUtil.getCursorRect(cursor);
  checkRect(expected, actual);

  cursor.index = 3;
  expected = {left: 126, top: 70, width: 1, height: 19};
  actual = SelectionUtil.getCursorRect(cursor);
  checkRect(expected, actual);

  const button = document.createElement('button');
  button.textContent = 'Get soundtrack';
  button.style = 'position: absolute; left: 10px; top: 0px';
  document.body.appendChild(button);

  cursor = new Cursor(button, 0, button.textContent);
  expected = {left: 10, top: 0, width: 1, height: 22};
  actual = SelectionUtil.getCursorRect(cursor);
  checkRect(expected, actual);
});

TEST_F('SelectionUtilUnitTest', 'IsAmbiguous', function() {
  const text =
      document.createTextNode('Raindrops on roses and whiskers on kittens');
  let selection = window.getSelection();
  selection.setBaseAndExtent(text, 13, text, 31); /* roses and whiskers */
  assertFalse(SelectionUtil.isAmbiguous(selection));

  selection.setBaseAndExtent(text, 31, text, 13);
  assertFalse(SelectionUtil.isAmbiguous(selection));

  // Mock a situation where the selection is ambiguous.
  selection = {
    anchorNode: text,
    baseNode: text,
    anchorOffset: 13,
    baseOffset: 31,
    focusNode: text,
    extentNode: text,
    focusOffset: 31,
    extentOffset: 13
  };
  assertTrue(SelectionUtil.isAmbiguous(selection));
});

TEST_F('SelectionUtilUnitTest', 'MakeCursor', function() {
  const text =
      document.createTextNode('Bright copper kettles and warm woolen mittens');
  document.body.appendChild(text);

  const selection = window.getSelection();
  selection.setBaseAndExtent(text, 7, text, 21); /* copper kettles */

  const leftCursor = new Cursor(text, 7, text.nodeValue);
  const rightCursor = new Cursor(text, 21, text.nodeValue);

  let actual = SelectionUtil.makeAnchorCursor(selection);
  checkCursor(leftCursor, actual);

  actual = SelectionUtil.makeFocusCursor(selection);
  checkCursor(rightCursor, actual);

  actual = SelectionUtil.makeLeftCursor(selection);
  checkCursor(leftCursor, actual);

  actual = SelectionUtil.makeRightCursor(selection);
  checkCursor(rightCursor, actual);

  // Reverse the focus and anchor.
  selection.setBaseAndExtent(text, 21, text, 7);

  actual = SelectionUtil.makeAnchorCursor(selection);
  checkCursor(rightCursor, actual);

  actual = SelectionUtil.makeFocusCursor(selection);
  checkCursor(leftCursor, actual);

  actual = SelectionUtil.makeLeftCursor(selection);
  checkCursor(leftCursor, actual);

  actual = SelectionUtil.makeRightCursor(selection);
  checkCursor(rightCursor, actual);
});

TEST_F('SelectionUtilUnitTest', 'SetAndValidateSelection', function() {
  const text =
      document.createTextNode('Brown paper packages tied up with strings');
  const start = new Cursor(text, 6, text.nodeValue);
  let end = new Cursor(text, 20, text.nodeValue);

  // Trying to set the selection to a node not in the document should return
  // false.
  assertFalse(SelectionUtil.setAndValidateSelection(start, end));

  document.body.appendChild(text);
  assertTrue(SelectionUtil.setAndValidateSelection(start, end));

  // Validate that the selection was set properly.
  const selection = window.getSelection();
  assertEquals(text, selection.anchorNode);
  assertEquals(6, selection.anchorOffset);
  assertEquals(text, selection.focusNode);
  assertEquals(20, selection.focusOffset);

  // Check that a selection across nodes works properly.
  const secondText =
      document.createTextNode('These are a few of my favorite things');
  document.body.appendChild(secondText);
  end = new Cursor(secondText, 5, secondText.nodeValue);
  assertTrue(SelectionUtil.setAndValidateSelection(start, end));

  assertEquals(text, selection.anchorNode);
  assertEquals(6, selection.anchorOffset);
  assertEquals(secondText, selection.focusNode);
  assertEquals(5, selection.focusOffset);
});

TEST_F('SelectionUtilUnitTest', 'IsCollapsed', function() {
  const text = document.createTextNode('When the dog bites');
  document.body.appendChild(text);

  const selection = window.getSelection();
  selection.setBaseAndExtent(text, 0, text, 0);
  assertTrue(SelectionUtil.isCollapsed(selection));

  selection.setBaseAndExtent(text, 0, text, 4);
  assertFalse(SelectionUtil.isCollapsed(selection));
});
