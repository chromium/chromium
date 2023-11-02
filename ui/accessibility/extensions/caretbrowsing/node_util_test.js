// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['node_util.js']);

GEN_INCLUDE(['../testing/webstore_extension_test_base.js']);

/** Test fixture for node_util.js. */
NodeUtilUnitTest = class extends WebstoreExtensionTest {};

TEST_F('NodeUtilUnitTest', 'IsFocusable', function() {
  assertFalse(NodeUtil.isFocusable(null));

  // Nodes with no tab index are not focusable.
  const noTabIndex = document.createElement('div');
  assertFalse(NodeUtil.isFocusable(noTabIndex));

  // Nodes with a positive number tab index are focusable.
  const positiveTabIndex = document.createElement('div');
  positiveTabIndex.tabIndex = 1;
  assertTrue(NodeUtil.isFocusable(positiveTabIndex));

  // Nodes with a tab index of 0 are focusable.
  const zeroTabIndex = document.createElement('div');
  zeroTabIndex.tabIndex = 0;
  assertTrue(NodeUtil.isFocusable(zeroTabIndex));

  // Some nodes have an implicit non-negative tab index.
  const button = document.createElement('button');
  assertTrue(NodeUtil.isFocusable(button));

  // Nodes with a negative tab index that's not -1 are not focusable.
  const negativeTwoTabIndex = document.createElement('button');
  negativeTwoTabIndex.tabIndex = -2;
  assertFalse(NodeUtil.isFocusable(negativeTwoTabIndex));

  // Nodes with an explicitly set tab index of -1 are focusable.
  const negativeOneDiv = document.createElement('div');
  negativeOneDiv.tabIndex = -1;
  assertTrue(NodeUtil.isFocusable(negativeOneDiv));

  const negativeOneButton = document.createElement('button');
  negativeOneButton.tabIndex = -1;
  assertTrue(NodeUtil.isFocusable(negativeOneButton));
});

TEST_F('NodeUtilUnitTest', 'IsDescendantOfNode', function() {
  const grandparent = document.createElement('div');
  const parent = document.createElement('p');
  const child = document.createElement('span');
  const orphan = document.createElement('img');

  grandparent.appendChild(parent);
  parent.appendChild(child);

  assertTrue(NodeUtil.isDescendantOfNode(parent, grandparent));
  assertTrue(NodeUtil.isDescendantOfNode(child, grandparent));
  assertTrue(NodeUtil.isDescendantOfNode(child, parent));

  assertFalse(NodeUtil.isDescendantOfNode(orphan, parent));
  assertFalse(NodeUtil.isDescendantOfNode(orphan, grandparent));
  assertFalse(NodeUtil.isDescendantOfNode(orphan, parent));

  assertFalse(NodeUtil.isDescendantOfNode(grandparent, parent));
  assertFalse(NodeUtil.isDescendantOfNode(grandparent, child));
  assertFalse(NodeUtil.isDescendantOfNode(parent, child));
  assertFalse(NodeUtil.isDescendantOfNode(grandparent, orphan));
  assertFalse(NodeUtil.isDescendantOfNode(parent, orphan));
  assertFalse(NodeUtil.isDescendantOfNode(child, orphan));
});

TEST_F('NodeUtilUnitTest', 'SetFocusToNode', function() {
  let focusCount;
  let selectCount;

  // Calling on a focusable node should focus that node.
  const button = document.createElement('button');
  button.focus = () => focusCount++;
  button.select = () => selectCount--;

  focusCount = 0;
  selectCount = 0;
  assertTrue(NodeUtil.setFocusToNode(button));
  assertEquals(1, focusCount);
  assertEquals(0, selectCount);

  // Calling on an iframe (even a focusable one) should not focus anything.
  const iframe = document.createElement('iframe');
  iframe.tabIndex = 0;
  iframe.focus = () => focusCount--;
  iframe.select = () => selectCount--;

  focusCount = 0;
  selectCount = 0;
  assertFalse(NodeUtil.setFocusToNode(iframe));
  assertEquals(0, focusCount);
  assertEquals(0, selectCount);

  // Calling on a focusable input should select the input contents.
  const input = document.createElement('input');
  input.focus = () => focusCount++;
  input.select = () => selectCount++;

  focusCount = 0;
  selectCount = 0;
  assertTrue(NodeUtil.setFocusToNode(input));
  assertEquals(1, focusCount);
  assertEquals(1, selectCount);

  // Calling on the child of a focusable node should focus the parent.
  const focusableDiv = document.createElement('div');
  focusableDiv.tabIndex = 0;
  focusableDiv.focus = () => focusCount++;
  focusableDiv.select = () => selectCount--;
  const p = document.createElement('p');
  p.focus = () => focusCount--;
  p.select = () => selectCount--;
  focusableDiv.appendChild(p);

  focusCount = 0;
  selectCount = 0;
  assertTrue(NodeUtil.setFocusToNode(p));
  assertEquals(1, focusCount);
  assertEquals(0, selectCount);

  // Calling on a non-focusable node with no ancestors other than document.body
  // should return false.
  const img = document.createElement('img');
  img.focus = () => focusCount--;
  img.select = () => selectCount--;
  document.body.appendChild(img);
  document.body.focus = () => focusCount--;
  document.body.select = () => selectCount--;

  focusCount = 0;
  selectCount = 0;
  assertFalse(NodeUtil.setFocusToNode(img));
  assertEquals(0, focusCount);
  assertEquals(0, selectCount);
});

TEST_F('NodeUtilUnitTest', 'SetFocusToFirstFocusable', function() {
  let focusACount = 0;
  let focusPCount = 0;
  let focusButtonCount = 0;

  const a = document.createElement('a');
  a.focus = () => focusACount++;
  const p = document.createElement('p');
  p.focus = () => focusPCount++;
  const button = document.createElement('button');
  button.focus = () => focusButtonCount++;

  assertTrue(NodeUtil.setFocusToFirstFocusable([a, p, button]));
  assertEquals(1, focusACount);
  assertEquals(0, focusPCount);
  assertEquals(0, focusButtonCount);

  focusACount = 0;
  assertTrue(NodeUtil.setFocusToFirstFocusable([p, button, a]));
  assertEquals(0, focusACount);
  assertEquals(0, focusPCount);
  assertEquals(1, focusButtonCount);

  // Passing in a list with no focusable elements returns false.
  assertFalse(NodeUtil.setFocusToFirstFocusable([p]));

  // Passing in an empty list returns false.
  assertFalse(NodeUtil.setFocusToFirstFocusable([]));
});
