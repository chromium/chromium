// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {XfTree} from './xf_tree.js';
import type {XfTreeItem} from './xf_tree_item.js';

/** Check if an `Element` is a tree or not. */
export function isTree(element: Element): element is XfTree {
  return element.tagName === 'XF-TREE';
}

/** Check if an `Element` is a tree item or not. */
export function isTreeItem(element: Element): element is XfTreeItem {
  return element.tagName === 'XF-TREE-ITEM';
}
