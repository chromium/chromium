// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';

import {type TreeSelectedChangedEvent, XfTree} from './xf_tree.js';
import {type TreeItemCollapsedEvent, type TreeItemExpandedEvent, XfTreeItem} from './xf_tree_item.js';

export function setUp() {
  document.body.innerHTML = getTrustedHTML`
    <xf-tree></xf-tree>
  `;
}

async function getTree(): Promise<XfTree> {
  const element = document.querySelector('xf-tree');
  assertNotEquals(null, element);
  await waitForElementUpdate(element!);
  return element!;
}

/** Helper method to get tree root <ul>. */
function getTreeRoot(tree: XfTree): HTMLUListElement {
  return tree.shadowRoot!.querySelector('ul')!;
}

/** Helper method to get tree item by id. */
function getTreeItemById(id: string): XfTreeItem {
  return document.querySelector(`xf-tree-item#${id}`)!;
}

function sendKeyDownEvent(tree: XfTree, key: string) {
  const keyDownEvent = new KeyboardEvent('keydown', {key});
  tree.dispatchEvent(keyDownEvent);
}

function simulateDoubleClick(element: HTMLElement) {
  element.dispatchEvent(new MouseEvent('dblclick', {
    bubbles: true,
    composed: true,
  }));
}

function simulateRightClick(element: HTMLElement) {
  element.dispatchEvent(new MouseEvent('mousedown', {
    button: 2,
    bubbles: true,
    composed: true,
  }));
}

/**
 * Helper method that checks that focused item is correct.
 */
function checkFocusedItemToBe(tree: XfTree, id: string): boolean {
  // Force focus the tree before checking document.activeElement. This is
  // because if the tree item itself is selected programmatically (e.g. via
  // ".selected = true"), the `.focusedItem` will update but it won't be
  // actually focused(). For more details check `Tree.makeItemFocusable_()`.
  tree.focus();
  return tree.focusedItem!.id === id && document.activeElement!.id === id;
}

/** Construct a tree with only direct children. */
async function appendDirectTreeItems(tree: XfTree) {
  // Tree structure:
  // ── item1
  // ── item2
  const item1 = document.createElement('xf-tree-item');
  item1.id = 'item1';
  item1.label = 'item1';
  const item2 = document.createElement('xf-tree-item');
  item2.id = 'item2';
  item2.label = 'item2';
  tree.appendChild(item1);
  tree.appendChild(item2);
  await waitForElementUpdate(tree);
}

/** Construct a tree with nested children. */
async function appendNestedTreeItems(tree: XfTree) {
  // Tree structure:
  // ── item1
  //    ├── item1a
  //    └── item1b
  //        └── item1bi
  // ── item2
  const item1 = document.createElement('xf-tree-item');
  item1.id = 'item1';
  item1.label = 'item1';
  const item1a = document.createElement('xf-tree-item');
  item1a.id = 'item1a';
  item1a.label = 'item1a';
  const item1b = document.createElement('xf-tree-item');
  item1b.id = 'item1b';
  item1b.label = 'item1b';
  const item1bi = document.createElement('xf-tree-item');
  item1bi.id = 'item1bi';
  item1bi.label = 'item1bi';
  const item2 = document.createElement('xf-tree-item');
  item2.id = 'item2';
  item2.label = 'item2';

  item1b.appendChild(item1bi);
  item1.appendChild(item1a);
  item1.appendChild(item1b);
  tree.appendChild(item1);
  tree.appendChild(item2);

  await waitForElementUpdate(tree);
}

/** Tests tree element can render without child tree items. */
export async function testRenderWithoutTreeItems() {
  const tree = await getTree();
  const treeRoot = getTreeRoot(tree);
  assertEquals('tree', treeRoot.getAttribute('role'));
  assertEquals('0', treeRoot.getAttribute('aria-setsize'));
  assertEquals(0, tree.items.length);
}

/** Tests tree element can render with child tree items. */
export async function testRenderWithTreeItems() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  const treeRoot = getTreeRoot(tree);
  assertEquals('2', treeRoot.getAttribute('aria-setsize'));
  assertEquals(2, tree.items.length);
  assertEquals('item1', tree.items[0]!.id);
  assertEquals('item2', tree.items[1]!.id);
}

/** Tests tree selection change. */
export async function testTreeSelectionChange() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Change at the tree item level.
  const selectionChangeEventPromise1: Promise<TreeSelectedChangedEvent> =
      eventToPromise(XfTree.events.TREE_SELECTION_CHANGED, tree);
  const item1 = getTreeItemById('item1');
  const item2 = getTreeItemById('item2');
  item1.selected = true;
  await waitForElementUpdate(tree);
  const selectionChangeEvent1 = await selectionChangeEventPromise1;
  assertEquals(item1, tree.selectedItem);
  assertEquals(null, selectionChangeEvent1.detail.previousSelectedItem);
  assertEquals(item1, selectionChangeEvent1.detail.selectedItem);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // Change at the tree level.
  const selectionChangeEventPromise2: Promise<TreeSelectedChangedEvent> =
      eventToPromise(XfTree.events.TREE_SELECTION_CHANGED, tree);
  tree.selectedItem = item2;
  const selectionChangeEvent2 = await selectionChangeEventPromise2;
  assertFalse(item1.selected);
  assertTrue(item2.selected);
  assertEquals(item1, selectionChangeEvent2.detail.previousSelectedItem);
  assertEquals(item2, selectionChangeEvent2.detail.selectedItem);
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
}

/** Tests tree item navigation by pressing home and end key. */
export async function testHomeAndEndNavigation() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  const item1bi = getTreeItemById('item1bi');
  // Expand item1 and item1b, then select item1bi.
  item1bi.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item1bi'));
  // Home -> item1.
  sendKeyDownEvent(tree, 'Home');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  // End -> item2.
  sendKeyDownEvent(tree, 'End');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
}

/** Tests tree item navigation by pressing arrow up and down key. */
export async function testArrowUpAndDownNavigation() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select and focus item1.
  const item1 = getTreeItemById('item1');
  item1.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // By default all items are collapsed.
  // ArrowDown -> item2.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
  // ArrowUp -> item1.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // Expand item1.
  item1.expanded = true;
  await waitForElementUpdate(tree);
  // ArrowDown -> item1a.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item1a'));
  // ArrowDown -> item1b.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));
  // ArrowDown -> item2.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
  // ArrowUp -> item1b.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));

  // Expand item1b.
  const item1b = getTreeItemById('item1b');
  item1b.expanded = true;
  await waitForElementUpdate(tree);
  // ArrowDown -> item1bi.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item1bi'));
  // ArrowDown -> item2.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
  // ArrowUp -> item1bi.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1bi'));
  // ArrowUp -> item1b.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));
  // ArrowUp -> item1a.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1a'));
  // ArrowUp -> item1.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

/** Tests no affect for arrow up key if tree item has no previous sibling. */
export async function testArrowUpForItemWithoutPreviousSibling() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item1 (no previous sibling).
  const item1 = getTreeItemById('item1');
  item1.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // ArrowUp -> item1 is still focused.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // ArrowUp again.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

/** Tests no affect for arrow down key if tree item has no next sibling. */
export async function testArrowDownForItemWithoutNextSibling() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item2 (no next sibling).
  const item2 = getTreeItemById('item2');
  item2.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item2'));

  // ArrowDown -> item2 is still focused.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));

  // ArrowDown again.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
}

/** Tests tree item expand/collapse by pressing arrow left and right key. */
export async function testArrowLeftAndRightNavigation() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item1.
  const item1 = getTreeItemById('item1');
  item1.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // ArrowRight -> expand item1.
  sendKeyDownEvent(tree, 'ArrowRight');
  await waitForElementUpdate(tree);
  assertTrue(item1.expanded);
  // Selected/focus item should not be changed.
  assertEquals(item1, tree.selectedItem);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  // ArrowRight -> focus first child item1a.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item1a'));
  // ArrowLeft -> item1.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  // Selected item and expand status should not be changed.
  assertEquals(item1, tree.selectedItem);
  assertTrue(item1.expanded);
  // ArrowRight -> item1a.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item1a'));
  // ArrowDown -> item1b.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));
  // ArrowRight -> expand item1b.
  sendKeyDownEvent(tree, 'ArrowRight');
  const item1b = getTreeItemById('item1b');
  await waitForElementUpdate(tree);
  assertTrue(item1b.expanded);
  // ArrowRight -> item1bi.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item1bi'));
  // Select item1bi.
  const item1bi = getTreeItemById('item1bi');
  item1bi.selected = true;
  await waitForElementUpdate(tree);
  // ArrowLeft -> item1b.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));
  assertTrue(item1b.expanded);
  assertFalse(item1b.selected);
  // ArrowLeft -> collapse item1b.
  sendKeyDownEvent(tree, 'ArrowLeft');
  await waitForElementUpdate(tree);
  assertFalse(item1b.expanded);
  // ArrowLeft -> item1.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  assertTrue(item1.expanded);
  // ArrowLeft -> collapse item1.
  sendKeyDownEvent(tree, 'ArrowLeft');
  await waitForElementUpdate(tree);
  assertFalse(item1.expanded);
}

/** Tests no affect for arrow left key if tree item has no parent. */
export async function testArrowLeftForItemWithoutParent() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item1 (no parent).
  const item1 = getTreeItemById('item1');
  item1.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // ArrowLeft -> item1 is still focused.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // ArrowLeft again.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

/** Tests no affect for arrow right key if tree item has no children. */
export async function testArrowRightForItemWithoutChildren() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item2 (no children).
  const item2 = getTreeItemById('item2');
  item2.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item2'));

  // ArrowRight -> item2 is still focused.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
  assertFalse(item2.expanded);

  // ArrowRight again.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
  assertFalse(item2.expanded);
}

/**
 * Tests tree item expand/collapse by pressing arrow left and right key in
 * RTL mode.
 */
export async function testArrowLeftAndRightNavigationInRTL() {
  document.documentElement.setAttribute('dir', 'rtl');
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item1.
  const item1 = getTreeItemById('item1');
  item1.selected = true;
  await waitForElementUpdate(tree);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // ArrowLeft -> expand item1.
  sendKeyDownEvent(tree, 'ArrowLeft');
  await waitForElementUpdate(tree);
  assertTrue(item1.expanded);
  // Selected/focus item should not be changed.
  assertEquals(item1, tree.selectedItem);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  // ArrowLeft -> focus first child item1a.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1a'));
  // ArrowRight -> item1.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  // Selected item and expand status should not be changed.
  assertEquals(item1, tree.selectedItem);
  assertTrue(item1.expanded);
  // ArrowLeft -> item1a.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1a'));
  // ArrowDown -> item1b.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));
  // ArrowLeft -> expand item1b.
  sendKeyDownEvent(tree, 'ArrowLeft');
  await waitForElementUpdate(tree);
  const item1b = getTreeItemById('item1b');
  assertTrue(item1b.expanded);
  // ArrowLeft -> item1bi.
  sendKeyDownEvent(tree, 'ArrowLeft');
  assertTrue(checkFocusedItemToBe(tree, 'item1bi'));
  // Select item1bi.
  const item1bi = getTreeItemById('item1bi');
  item1bi.selected = true;
  await waitForElementUpdate(tree);
  // ArrowRight -> item1b.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));
  assertTrue(item1b.expanded);
  assertFalse(item1b.selected);
  // ArrowRight -> collapse item1b.
  sendKeyDownEvent(tree, 'ArrowRight');
  await waitForElementUpdate(tree);
  assertFalse(item1b.expanded);
  // ArrowRight -> item1.
  sendKeyDownEvent(tree, 'ArrowRight');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  assertTrue(item1.expanded);
  // ArrowRight -> collapse item1.
  sendKeyDownEvent(tree, 'ArrowRight');
  await waitForElementUpdate(tree);
  assertFalse(item1.expanded);

  document.documentElement.removeAttribute('dir');
}

/** Tests tree item selection by pressing Enter/Space key. */
export async function testEnterToSelectItem() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  const item1 = getTreeItemById('item1');
  const item2 = getTreeItemById('item2');
  item1.selected = true;
  await waitForElementUpdate(tree);

  // Use Enter to select item2.
  const selectionChangeEventPromise1: Promise<TreeSelectedChangedEvent> =
      eventToPromise(XfTree.events.TREE_SELECTION_CHANGED, tree);
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
  sendKeyDownEvent(tree, 'Enter');
  await waitForElementUpdate(tree);
  const selectionChangeEvent1 = await selectionChangeEventPromise1;
  assertTrue(item2.selected);
  assertEquals(item2, tree.selectedItem);
  assertEquals(item1, selectionChangeEvent1.detail.previousSelectedItem);
  assertEquals(item2, selectionChangeEvent1.detail.selectedItem);

  // Use Space to select item1.
  const selectionChangeEventPromise2: Promise<TreeSelectedChangedEvent> =
      eventToPromise(XfTree.events.TREE_SELECTION_CHANGED, tree);
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
  sendKeyDownEvent(tree, ' ');
  await waitForElementUpdate(tree);
  const selectionChangeEvent2 = await selectionChangeEventPromise2;
  assertTrue(item1.selected);
  assertEquals(item1, tree.selectedItem);
  assertEquals(item2, selectionChangeEvent2.detail.previousSelectedItem);
  assertEquals(item1, selectionChangeEvent2.detail.selectedItem);
}

/** Tests tree item can be expanded by single click. */
export async function testExpandTreeItemByClick() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Single click on the expand-icon.
  const item1 = getTreeItemById('item1');
  const expandIcon =
      item1.shadowRoot!.querySelector<HTMLSpanElement>('.expand-icon')!;
  expandIcon.click();
  await waitForElementUpdate(item1);

  // item1 should be expanded, not selected.
  assertTrue(item1.expanded);
  assertFalse(item1.selected);

  // Single click again on the expand-icon.
  expandIcon.click();
  await waitForElementUpdate(item1);

  // item1 should be collapsed, not selected.
  assertFalse(item1.expanded);
  assertFalse(item1.selected);
}

/** Tests tree item can be selected by single click. */
export async function testSelectTreeItemByClick() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Single click on item1.
  const item1 = getTreeItemById('item1');
  item1.click();
  await waitForElementUpdate(item1);

  // item1 should be selected, not expanded.
  assertFalse(item1.expanded);
  assertTrue(item1.selected);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

/** Tests tree item can be expanded by double click. */
export async function testExpandTreeItemByDoubleClick() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Double click on item1.
  const item1 = getTreeItemById('item1');
  simulateDoubleClick(item1);
  await waitForElementUpdate(item1);

  // item1 should be expanded.
  assertTrue(item1.expanded);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // Double click again on item1.
  simulateDoubleClick(item1);
  await waitForElementUpdate(item1);

  // item1 should be collapsed.
  assertFalse(item1.expanded);
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

/** Tests tree item can be focused by right click. */
export async function testFocusTreeItemByRightClick() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Right click on item1.
  const item1 = getTreeItemById('item1');
  simulateRightClick(item1);
  await waitForElementUpdate(item1);

  // item1 should be focused.
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

export async function testClickHostShouldFocusItem() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Make item2 focusable.
  const item2 = getTreeItemById('item2');
  tree.focusedItem = item2;

  // item2 should not be focused yet.
  assertNotEquals('item2', document.activeElement!.id);

  // Click the tree will make item2 become focused.
  tree.click();
  assertEquals('item2', document.activeElement!.id);
}

export async function testRightClickHostShouldFocusItem() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Make item2 focusable.
  const item2 = getTreeItemById('item2');
  tree.focusedItem = item2;

  // item2 should not be focused yet.
  assertNotEquals('item2', document.activeElement!.id);

  // Right click the tree will make item2 become focused.
  simulateRightClick(tree);
  assertEquals('item2', document.activeElement!.id);
}

export async function testDoubleClickHostShouldFocusItem() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Make item2 focusable.
  const item2 = getTreeItemById('item2');
  tree.focusedItem = item2;

  // item2 should not be focused yet.
  assertNotEquals('item2', document.activeElement!.id);

  // Double click the tree will make item2 become focused.
  simulateDoubleClick(tree);
  assertEquals('item2', document.activeElement!.id);
}

/** Tests disabled tree item should be skipped for navigation. */
export async function testSkipDisabledItem() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select and focus item1, then expand it.
  const item1 = getTreeItemById('item1');
  item1.selected = true;
  item1.expanded = true;
  // Disable item1a.
  const item1a = getTreeItemById('item1a');
  item1a.disabled = true;
  await waitForElementUpdate(tree);

  // ArrowDown -> item1b, skip item1a.
  sendKeyDownEvent(tree, 'ArrowDown');
  assertTrue(checkFocusedItemToBe(tree, 'item1b'));
  // ArrowUp -> item1, skip item1a.
  sendKeyDownEvent(tree, 'ArrowUp');
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

/** Tests click/double click on the disabled item has no effects. */
export async function testNoActionOnDisabledItem() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item2.
  const item2 = getTreeItemById('item2');
  item2.selected = true;
  await waitForElementUpdate(item2);

  // Disable item1.
  const item1 = getTreeItemById('item1');
  item1.disabled = true;
  await waitForElementUpdate(item1);


  // No response for single click.
  assertFalse(item1.selected);
  item1.click();
  await waitForElementUpdate(item1);
  assertFalse(item1.selected);
  assertTrue(item2.selected);

  // No response for single click the hidden expand icon.
  assertFalse(item1.expanded);
  const expandIcon =
      item1.shadowRoot!.querySelector<HTMLSpanElement>('.expand-icon')!;
  expandIcon.click();
  await waitForElementUpdate(item1);
  assertFalse(item1.expanded);

  // No response for double click.
  assertFalse(item1.expanded);
  simulateDoubleClick(item1);
  await waitForElementUpdate(item1);
  assertFalse(item1.expanded);
}

/** Tests adding/removing tree items. */
export async function testAddRemoveTreeItems() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Add a new tree item: item3.
  const item3 = document.createElement('xf-tree-item');
  item3.id = 'item3';
  tree.appendChild(item3);
  await waitForElementUpdate(tree);
  assertEquals('3', getTreeRoot(tree).getAttribute('aria-setsize'));
  assertEquals(3, tree.items.length);
  assertEquals('item3', tree.items[2]!.id);

  // Remove tree item: item2.
  const item2 = getTreeItemById('item2');
  tree.removeChild(item2);
  await waitForElementUpdate(tree);
  assertEquals('2', getTreeRoot(tree).getAttribute('aria-setsize'));
  assertEquals(2, tree.items.length);
  assertEquals('item1', tree.items[0]!.id);
  assertEquals('item3', tree.items[1]!.id);
}

/** Tests removing a selected tree item should update selectedItem properly. */
export async function testSelectionUpdateAfterRemoving() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Select item2.
  const item2 = getTreeItemById('item2');
  item2.selected = true;
  await waitForElementUpdate(tree);

  // Remove item2.
  tree.removeChild(item2);
  await waitForElementUpdate(tree);

  // The selected item should be null now.
  assertEquals(null, tree.selectedItem);
}

/** Tests removing a focused tree item should update focusedItem properly. */
export async function testFocusUpdateAfterRemoving() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Focus item2.
  const item2 = getTreeItemById('item2');
  tree.focusedItem = item2;

  // Select item1.
  const item1 = getTreeItemById('item1');
  item1.selected = true;
  await waitForElementUpdate(item1);

  // Remove item2.
  tree.removeChild(item2);
  await waitForElementUpdate(tree);

  // The selected item should be the selected item now.
  assertEquals('item1', tree.focusedItem.id);
}

/** Tests tree should be able to observe tree item event. */
export async function testObserveTreeItemEvent() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Expand item1.
  const itemExpandedEventPromise: Promise<TreeItemExpandedEvent> =
      eventToPromise(XfTreeItem.events.TREE_ITEM_EXPANDED, tree);
  const item1 = getTreeItemById('item1');
  item1.expanded = true;
  await waitForElementUpdate(tree);
  const itemExpandedEvent = await itemExpandedEventPromise;
  assertEquals(item1, itemExpandedEvent.detail.item);

  // Collapse item1.
  const itemCollapsedEventPromise: Promise<TreeItemCollapsedEvent> =
      eventToPromise(XfTreeItem.events.TREE_ITEM_COLLAPSED, tree);
  item1.expanded = false;
  await waitForElementUpdate(tree);
  const itemCollapsedEvent = await itemCollapsedEventPromise;
  assertEquals(item1, itemCollapsedEvent.detail.item);
}

/**
 * Tests focus will move to its parent if the focused tree item is collapsed.
 */
export async function testFocusMoveToParentIfCollapsed() {
  const tree = await getTree();
  await appendNestedTreeItems(tree);

  // Select item1b.
  const item1b = getTreeItemById('item1b');
  item1b.selected = true;
  await waitForElementUpdate(tree);

  // Collapse item1b's parent item1.
  const item1 = getTreeItemById('item1');
  assertTrue(item1.expanded);
  item1.expanded = false;
  await waitForElementUpdate(tree);

  // Focus should move to item1.
  assertTrue(checkFocusedItemToBe(tree, 'item1'));
}

/** Tests tree item can be focused directly via focus() method. */
export async function testFocusItemViaFocusMethod() {
  const tree = await getTree();
  await appendDirectTreeItems(tree);

  // Focus item1.
  const item1 = getTreeItemById('item1');
  item1.focus();
  assertTrue(checkFocusedItemToBe(tree, 'item1'));

  // Focus item2.
  const item2 = getTreeItemById('item2');
  item2.focus();
  assertTrue(checkFocusedItemToBe(tree, 'item2'));
}
