// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {XfTree} from './xf_tree.js';
import type {XfTreeItem} from './xf_tree_item.js';

/** Check if an `Element` is a tree or not. */
export function isXfTree(element: HTMLElement|Element|EventTarget|undefined|
                         null): element is XfTree {
  return !!element && 'tagName' in element && element.tagName === 'XF-TREE';
}

/** Check if an `Element` is a tree item or not. */
export function isTreeItem(element: HTMLElement|Element|EventTarget|undefined|
                           null): element is XfTreeItem {
  return !!element && 'tagName' in element &&
      element.tagName === 'XF-TREE-ITEM';
}

/**
 * When tree slot or tree item's slot changes, we need to check if the change
 * impacts the selected item and focused item or not, if so we update the
 * `selectedItem/focusedItem` in the tree.
 */
export function handleTreeSlotChange(
    tree: XfTree, oldItems: Set<XfTreeItem>, newItems: Set<XfTreeItem>) {
  if (tree.selectedItem) {
    if (oldItems.has(tree.selectedItem) && !newItems.has(tree.selectedItem)) {
      // If the currently selected item exists in `oldItems` but not in
      // `newItems`, it means it's being removed from the children slot,
      // we need to mark the selected item to null.
      tree.selectedItem = null;
    }
  }

  if (tree.focusedItem) {
    if (oldItems.has(tree.focusedItem) && !newItems.has(tree.focusedItem)) {
      // If the currently focused item exists in `oldItems` but not in
      // `newItems`, it means it's being removed from the children slot,
      // we need to mark the focused item to the currently selected item.
      tree.focusedItem = tree.selectedItem;
    }
  }
}
