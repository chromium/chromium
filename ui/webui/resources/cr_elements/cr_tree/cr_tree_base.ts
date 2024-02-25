// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {CustomElement} from '//resources/js/custom_element.js';

export const EXPANDED_ATTR: string = 'expanded';

// Encapuslates shared behavior of cr-trees and the cr-tree-items that they
// contain. This reduces code duplication for e.g. adding/removing children and
// facilitates writing methods navigating the full tree structure (cr-tree and
// all cr-tree-item descendants), without introducing circular dependencies.
export abstract class CrTreeBaseElement extends CustomElement {
  static override get template() {
    return window.trustedTypes ? window.trustedTypes.emptyHTML : ('' as string);
  }

  static get observedAttributes() {
    return ['icon-visibility'];
  }

  detail: object = {};
  private parent_: CrTreeBaseElement|null = null;

  attributeChangedCallback(name: string, _oldValue: string, newValue: string) {
    assert(name === 'icon-visibility');
    this.items.forEach(item => item.setAttribute(name, newValue));
  }

  setParent(parent: CrTreeBaseElement) {
    this.parent_ = parent;
  }

  get items(): CrTreeBaseElement[] {
    return Array.from(
        this.itemsRoot.querySelectorAll<CrTreeBaseElement>('cr-tree-item'));
  }

  abstract get depth(): number;
  abstract set depth(depth: number);
  abstract get itemsRoot(): DocumentFragment|HTMLElement;
  abstract get selectedItem(): CrTreeBaseElement|null;
  abstract set selectedItem(item: CrTreeBaseElement|null);

  /**
   * Adds a tree item as a child.
   */
  add(child: CrTreeBaseElement) {
    this.addAt(child, -1);
  }

  /**
   * Adds a tree item as a child at a given index.
   */
  addAt(child: CrTreeBaseElement, index: number) {
    assert(child.tagName === 'CR-TREE-ITEM');
    child.setParent(this);

    if (index === -1 || index >= this.items.length) {
      this.itemsRoot.appendChild(child);
    } else {
      this.itemsRoot.insertBefore(child, this.items[index] || null);
    }
    if (this.items.length === 1) {
      this.setHasChildren(true);
    }
    child.depth = this.depth + 1;
    child.setAttribute(
        'icon-visibility', this.getAttribute('icon-visibility') || '');
  }

  removeTreeItem(child: CrTreeBaseElement) {
    this.itemsRoot.removeChild(child);
    if (this.items.length === 0) {
      this.setHasChildren(false);
    }
  }

  get parentItem(): CrTreeBaseElement|null {
    return this.parent_;
  }

  /**
   * The tree that the tree item belongs to or null of no added to a tree.
   */
  get tree(): CrTreeBaseElement|null {
    if (this.tagName === 'CR-TREE') {
      return this;
    }

    if (!this.parent_) {
      return null;
    }

    return this.parent_.tree;
  }

  get hasChildren(): boolean {
    return !!this.items[0];
  }

  setHasChildren(b: boolean) {
    this.toggleAttribute('has-children', b);
  }

  get expanded() {
    return this.hasAttribute(EXPANDED_ATTR);
  }

  set expanded(expanded: boolean) {
    this.toggleAttribute(EXPANDED_ATTR, expanded);
  }
}
