// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTrustedHTML} from 'chrome://resources/js/static_types.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {waitForElementUpdate} from '../common/js/unittest_util.js';
import {ICON_TYPES} from '../foreground/js/constants.js';

import type {XfIcon} from './xf_icon.js';
import type {XfTree} from './xf_tree.js';
import {TREE_ITEM_INDENT, type TreeItemCollapsedEvent, type TreeItemExpandedEvent, XfTreeItem} from './xf_tree_item.js';

/** Construct a single tree item. */
async function setUpSingleTreeItem() {
  document.body.innerHTML = getTrustedHTML`
    <xf-tree>
      <xf-tree-item id="item1" label="item1"></xf-tree-item>
    </xf-tree>
  `;
  const element = document.querySelector('xf-tree-item');
  assertNotEquals(null, element);
  await waitForElementUpdate(element!);
}

/** Construct a tree with nested tree items. */
async function setUpNestedTreeItems() {
  // Tree structure:
  // ── item1
  //    ├── item1a
  //    └── item1b
  //        └── item1bi
  // ── item2
  document.body.innerHTML = getTrustedHTML`
    <xf-tree><xf-tree>
  `;
  const tree = document.querySelector('xf-tree')!;
  assertNotEquals(null, tree);

  const item1 = document.createElement('xf-tree-item');
  item1.id = 'item1';
  const item1a = document.createElement('xf-tree-item');
  item1a.id = 'item1a';
  const item1b = document.createElement('xf-tree-item');
  item1b.id = 'item1b';
  const item1bi = document.createElement('xf-tree-item');
  item1bi.id = 'item1bi';
  const item2 = document.createElement('xf-tree-item');
  item2.id = 'item2';

  item1b.appendChild(item1bi);
  item1.appendChild(item1a);
  item1.appendChild(item1b);
  tree.appendChild(item1);
  tree.appendChild(item2);

  await waitForElementUpdate(tree);
}

/** Helper method to get tree item by id. */
function getTree(): XfTree {
  return document.querySelector('xf-tree')!;
}

/** Helper method to get tree item by id. */
function getTreeItemById(id: string): XfTreeItem {
  return document.querySelector(`xf-tree-item#${id}`)!;
}

/** Helper method to get inner elements from a tree item. */
function getTreeItemInnerElements(treeItem: XfTreeItem): {
  root: HTMLLIElement,
  treeRow: HTMLDivElement,
  expandIcon: HTMLSpanElement,
  treeLabel: HTMLSpanElement,
  treeLabelIcon: XfIcon,
  trailingIcon: HTMLSlotElement,
  treeChildren: HTMLUListElement,
} {
  return {
    root: treeItem.shadowRoot!.querySelector('li')!,
    treeRow: treeItem.shadowRoot!.querySelector('.tree-row')!,
    expandIcon: treeItem.shadowRoot!.querySelector('.expand-icon')!,
    treeLabel: treeItem.shadowRoot!.querySelector('.tree-label')!,
    treeLabelIcon: treeItem.shadowRoot!.querySelector('.tree-label-icon')!,
    trailingIcon:
        treeItem.shadowRoot!.querySelector('slot[name="trailingIcon"]')!,
    treeChildren: treeItem.shadowRoot!.querySelector('.tree-children')!,
  };
}

/** Tests tree item can be rendered without tree or child tree items. */
export async function testRenderWithSingleTreeItem(done: () => void) {
  await setUpSingleTreeItem();
  const item1 = getTreeItemById('item1');
  const {root, treeRow, expandIcon, treeLabel, treeChildren} =
      getTreeItemInnerElements(item1);

  // Check item1's parent/children.
  assertEquals(1, item1.level);
  assertEquals(0, item1.items.length);
  assertEquals(null, item1.parentItem);
  assertEquals(getTree(), item1.tree);

  // Test attributes on the root element.
  assertEquals('treeitem', root.getAttribute('role'));
  assertEquals('false', root.getAttribute('aria-selected'));
  assertFalse(root.hasAttribute('aria-expanded'));
  assertEquals('false', root.getAttribute('aria-disabled'));
  assertEquals(treeLabel.id, root.getAttribute('aria-labelledby'));

  // Test inner elements.
  assertEquals('0px', window.getComputedStyle(treeRow).paddingInlineStart);
  assertEquals('hidden', window.getComputedStyle(expandIcon).visibility);
  assertEquals('item1', treeLabel.textContent);
  assertEquals('group', treeChildren.getAttribute('role'));

  done();
}

/** Tests tree item can be rendered with child tree items. */
export async function testRenderWithTreeItems(done: () => void) {
  await setUpNestedTreeItems();
  const tree = getTree();

  // Check item1's parent/children.
  const item1 = getTreeItemById('item1');
  const {root: root1, expandIcon: expandIcon1} =
      getTreeItemInnerElements(item1);
  assertEquals('false', root1.getAttribute('aria-expanded'));
  assertEquals(2, item1.items.length);
  assertEquals('item1a', item1.items[0]!.id);
  assertEquals('item1b', item1.items[1]!.id);
  assertEquals(null, item1.parentItem);
  assertEquals(tree, item1.tree);
  assertEquals('visible', window.getComputedStyle(expandIcon1).visibility);

  // Check item1b's parent/children.
  const item1b = getTreeItemById('item1b');
  const {root: root1b, expandIcon: expandIcon1b} =
      getTreeItemInnerElements(item1);
  assertEquals('false', root1b.getAttribute('aria-expanded'));
  assertEquals(1, item1b.items.length);
  assertEquals('item1bi', item1b.items[0]!.id);
  assertEquals(item1, item1b.parentItem);
  assertEquals(tree, item1b.tree);
  assertEquals('visible', window.getComputedStyle(expandIcon1b).visibility);

  done();
}

/** Tests "may-have-children" attribute. */
export async function testMayHaveChildrenAttribute(done: () => void) {
  await setUpSingleTreeItem();

  const item1 = getTreeItemById('item1');
  const {root} = getTreeItemInnerElements(item1);
  // Expand icon is hidden by default (no aria-expanded).
  assertFalse(root.hasAttribute('aria-expanded'));
  // Set may-have-children=true.
  item1.mayHaveChildren = true;
  await waitForElementUpdate(item1);
  // Expand-icon should be visible now (has aria-expanded).
  assertTrue(root.hasAttribute('aria-expanded'));
  assertTrue(item1.hasChildren());

  done();
}

/** Tests tree item level will be correctly updated. */
export async function testTreeItemLevel(done: () => void) {
  await setUpNestedTreeItems();

  const item1 = getTreeItemById('item1');
  const item1a = getTreeItemById('item1a');
  const item1b = getTreeItemById('item1b');
  const item1bi = getTreeItemById('item1bi');
  const item2 = getTreeItemById('item2');

  const tree = getTree();
  tree.style.setProperty('--xf-tree-item-indent', TREE_ITEM_INDENT.toString());

  const {treeRow: treeRow1} = getTreeItemInnerElements(item1);
  assertEquals(1, item1.level);
  assertEquals('0px', window.getComputedStyle(treeRow1).paddingInlineStart);

  const {treeRow: treeRow1a} = getTreeItemInnerElements(item1a);
  assertEquals(2, item1a.level);
  assertEquals(
      `${TREE_ITEM_INDENT}px`,
      window.getComputedStyle(treeRow1a).paddingInlineStart);

  const {treeRow: treeRow1b} = getTreeItemInnerElements(item1b);
  assertEquals(2, item1b.level);
  assertEquals(
      `${TREE_ITEM_INDENT}px`,
      window.getComputedStyle(treeRow1b).paddingInlineStart);

  const {treeRow: treeRow1bi} = getTreeItemInnerElements(item1bi);
  assertEquals(3, item1bi.level);
  assertEquals(
      `${TREE_ITEM_INDENT * 2}px`,
      window.getComputedStyle(treeRow1bi).paddingInlineStart);

  const {treeRow: treeRow2} = getTreeItemInnerElements(item2);
  assertEquals(1, item2.level);
  assertEquals('0px', window.getComputedStyle(treeRow2).paddingInlineStart);

  done();
}

/** Tests trialing icon can be rendered correctly. */
export async function testTrailingIcon(done: () => void) {
  await setUpSingleTreeItem();
  const item1 = getTreeItemById('item1');

  // Add a trailing icon for item1.
  const icon = document.createElement('span');
  icon.slot = 'trailingIcon';
  item1.appendChild(icon);

  const {trailingIcon} = getTreeItemInnerElements(item1);
  const slotElements = trailingIcon.assignedElements();
  assertEquals(1, slotElements.length);
  assertEquals(icon, slotElements[0]);

  done();
}

/**
 * Tests disabled tree item won't be included in the tabbable items.
 */
export async function testDisabledTreeItem(done: () => void) {
  await setUpNestedTreeItems();

  // By default item1 has 2 tabbable items.
  const item1 = getTreeItemById('item1');
  assertEquals(2, item1.tabbableItems.length);

  // Disable item1b.
  const item1b = getTreeItemById('item1b');
  item1b.disabled = true;
  await waitForElementUpdate(item1b);

  // aria-disabled should be true and expand icon should be hidden.
  const {root, expandIcon} = getTreeItemInnerElements(item1b);
  assertEquals('true', root.getAttribute('aria-disabled'));
  assertEquals('hidden', window.getComputedStyle(expandIcon).visibility);

  // item1b will be ignored in tabbable items.
  assertEquals(2, item1.items.length);
  assertEquals(1, item1.tabbableItems.length);
  assertEquals('item1a', item1.tabbableItems[0]!.id);

  done();
}

/** Tests tree item can be selected. */
export async function testSelectTreeItem(done: () => void) {
  await setUpNestedTreeItems();

  // Select item1bi.
  const item1bi = getTreeItemById('item1bi');
  item1bi.selected = true;
  await waitForElementUpdate(item1bi);

  const {root} = getTreeItemInnerElements(item1bi);
  assertEquals('true', root.getAttribute('aria-selected'));
  const tree = getTree();
  assertEquals('item1bi', tree.selectedItem!.id);

  // All its parent chain will be expanded.
  const item1b = getTreeItemById('item1b');
  const item1 = getTreeItemById('item1');
  assertTrue(item1b.expanded);
  assertTrue(item1.expanded);

  // Unselect item1bi.
  item1bi.selected = false;
  await waitForElementUpdate(item1bi);

  assertEquals('false', root.getAttribute('aria-selected'));
  assertEquals(null, tree.selectedItem);

  done();
}


/** Tests tree item can be expanded. */
export async function testExpandTreeItem(done: () => void) {
  await setUpNestedTreeItems();

  // By default children items are not displayed.
  const item1 = getTreeItemById('item1');
  const {treeChildren} = getTreeItemInnerElements(item1);
  assertEquals('none', window.getComputedStyle(treeChildren).display);

  // Expand item1.
  const itemExpandedEventPromise: Promise<TreeItemExpandedEvent> =
      eventToPromise(XfTreeItem.events.TREE_ITEM_EXPANDED, item1);
  item1.expanded = true;
  await waitForElementUpdate(item1);
  const {root} = getTreeItemInnerElements(item1);
  assertEquals('true', root.getAttribute('aria-expanded'));

  // Assert the event is triggered.
  const itemExpandedEvent = await itemExpandedEventPromise;
  assertEquals(item1, itemExpandedEvent.detail.item);

  // Assert the children items are shown.
  assertEquals('block', window.getComputedStyle(treeChildren).display);

  done();
}

/**
 * Tests tree item can be collapsed.
 */
export async function testCollapseTreeItem(done: () => void) {
  await setUpNestedTreeItems();

  // Select item1b.
  const item1b = getTreeItemById('item1b');
  item1b.selected = true;
  await waitForElementUpdate(item1b);

  const item1 = getTreeItemById('item1');
  const {treeChildren} = getTreeItemInnerElements(item1);
  await waitForElementUpdate(item1);
  assertTrue(item1.expanded);

  // Collapse item1.
  const itemCollapsedEventPromise: Promise<TreeItemCollapsedEvent> =
      eventToPromise(XfTreeItem.events.TREE_ITEM_COLLAPSED, item1);
  item1.expanded = false;
  await waitForElementUpdate(item1);
  const {root} = getTreeItemInnerElements(item1);
  assertEquals('false', root.getAttribute('aria-expanded'));

  // Assert the event is triggered.
  const itemCollapsedEvent = await itemCollapsedEventPromise;
  assertEquals(item1, itemCollapsedEvent.detail.item);

  // Assert the children items are hidden.
  assertEquals('none', window.getComputedStyle(treeChildren).display);

  done();
}

/** Tests adding/removing tree items. */
export async function testAddRemoveTreeItems(done: () => void) {
  await setUpSingleTreeItem();
  const item1 = getTreeItemById('item1');

  // Add item1a as a child to item1.
  const item1a = document.createElement('xf-tree-item');
  item1a.id = 'item1a';
  item1.appendChild(item1a);
  await waitForElementUpdate(item1);
  assertEquals(1, item1.items.length);
  assertEquals('item1a', item1.items[0]!.id);

  // Add item1b as a child to item1.
  const item1b = document.createElement('xf-tree-item');
  item1b.id = 'item1b';
  item1.appendChild(item1b);
  await waitForElementUpdate(item1);
  assertEquals(2, item1.items.length);
  assertEquals('item1b', item1.items[1]!.id);

  // Remove item1a.
  item1.removeChild(item1a);
  await waitForElementUpdate(item1);
  assertEquals(1, item1.items.length);
  assertEquals('item1b', item1.items[0]!.id);

  done();
}

/** Tests expanded item will become collapsed when last child is removed. */
export async function testRemoveChildForExpandedItem(done: () => void) {
  await setUpNestedTreeItems();

  // Expand item1.
  const item1 = getTreeItemById('item1');
  item1.expanded = true;
  await waitForElementUpdate(item1);

  // Remove item1a.
  const item1a = getTreeItemById('item1a');
  item1.removeChild(item1a);
  await waitForElementUpdate(item1);
  assertTrue(item1.expanded);

  // Remove item1b.
  const item1b = getTreeItemById('item1b');
  item1.removeChild(item1b);
  await waitForElementUpdate(item1);

  // item1 will be collapsed because all its children are removed.
  assertFalse(item1.expanded);

  done();
}

/** Tests removal of the selected item. */
export async function testRemoveSelectedItem(done: () => void) {
  await setUpNestedTreeItems();

  // Select item1a.
  const item1a = getTreeItemById('item1a');
  item1a.selected = true;
  await waitForElementUpdate(item1a);

  // Remove item1a.
  const item1 = getTreeItemById('item1');
  assertFalse(item1.selected);
  item1.removeChild(item1a);
  await waitForElementUpdate(item1);

  // The selected item should be null now.
  assertEquals(null, item1.tree?.selectedItem);

  done();
}

/** Tests removal of the focused item. */
export async function testRemoveFocusedItem(done: () => void) {
  await setUpNestedTreeItems();
  const tree = getTree();

  // Focus item1a.
  const item1a = getTreeItemById('item1a');
  tree.focusedItem = item1a;

  // Select item1b.
  const item1b = getTreeItemById('item1b');
  item1b.selected = true;
  await waitForElementUpdate(item1b);

  // Remove item1a.
  const item1 = getTreeItemById('item1');
  item1.removeChild(item1a);
  await waitForElementUpdate(item1);

  // The focused item should be the selected item now.
  assertEquals('item1b', tree.focusedItem.id);

  done();
}

/** Tests that iconSet has higher priority than icon property. */
export async function testIconSetIgnoreIcon(done: () => void) {
  await setUpSingleTreeItem();

  // Set both icon and iconSet.
  const item1 = getTreeItemById('item1');
  item1.icon = ICON_TYPES.ANDROID_FILES;
  item1.iconSet = {
    icon16x16Url: undefined,
    icon32x32Url: 'fake-base64-data',
  };
  await waitForElementUpdate(item1);

  // Check only iconSet property is set for the xf-icon.
  const {treeLabelIcon} = getTreeItemInnerElements(item1);
  assertEquals(null, treeLabelIcon.type);
  assertEquals('fake-base64-data', treeLabelIcon.iconSet!.icon32x32Url);

  done();
}

/** Tests the has-children attribute. */
export async function testHasChildrenAttribute(done: () => void) {
  await setUpSingleTreeItem();
  const item1 = getTreeItemById('item1');

  // Check has-children attribute is false because we have no children.
  assertEquals('false', item1.getAttribute('has-children'));

  // Add a child item for item1.
  const item1a = document.createElement('xf-tree-item');
  item1a.id = 'item1a';
  item1.appendChild(item1a);
  await waitForElementUpdate(item1);

  // Check has-children attribute is true now because we have 1 child now.
  assertEquals('true', item1.getAttribute('has-children'));

  done();
}
