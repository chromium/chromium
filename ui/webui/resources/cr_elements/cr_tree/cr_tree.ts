// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from '//resources/js/assert.js';
import {isMac} from '//resources/js/platform.js';

import {getTemplate} from './cr_tree.html.js';
import {CrTreeBaseElement} from './cr_tree_base.js';
import type {CrTreeItemElement} from './cr_tree_item.js';
import {SELECTED_ATTR} from './cr_tree_item.js';

/**
 * @fileoverview cr-tree is a container for a tree structure. Items can be added
 * or removed from the tree using the add/addAt/removeItem methods. Adding items
 * declaratively is not currently supported, as this class is primarily intended
 * to replace cr.ui.Tree, which is used for cases of creating trees at runtime
 * (e.g. from backend data).
 */

/**
 * Helper function that returns the next visible tree item.
 */
function getNext(item: CrTreeBaseElement): CrTreeBaseElement|null {
  if (item.expanded) {
    const firstChild = item.items[0];
    if (firstChild) {
      return firstChild;
    }
  }

  return getNextHelper(item);
}

/**
 * Another helper function that returns the next visible tree item.
 */
function getNextHelper(item: CrTreeBaseElement|null): CrTreeBaseElement|null {
  if (!item) {
    return null;
  }

  const nextSibling = item.nextElementSibling;
  if (nextSibling) {
    assert(nextSibling.tagName === 'CR-TREE-ITEM');
    return nextSibling as CrTreeBaseElement;
  }
  const parent = item.parentItem;
  if (!parent || parent.tagName === 'CR-TREE') {
    return null;
  }
  return getNextHelper(item.parentItem);
}

/**
 * Helper function that returns the previous visible tree item.
 */
function getPrevious(item: CrTreeBaseElement): CrTreeBaseElement|null {
  const previousSibling = item.previousElementSibling;
  if (previousSibling && previousSibling.tagName === 'CR-TREE-ITEM') {
    return getLastHelper(previousSibling as CrTreeBaseElement);
  }
  return item.parentItem;
}

/**
 * Helper function that returns the last visible tree item in the subtree.
 */
function getLastHelper(item: CrTreeBaseElement): CrTreeBaseElement|null {
  if (item.expanded && item.hasChildren) {
    const lastChild = item.items[item.items.length - 1]!;
    return getLastHelper(lastChild);
  }
  return item;
}

export class CrTreeElement extends CrTreeBaseElement {
  static override get template() {
    return getTemplate();
  }

  private selectedItem_: CrTreeBaseElement|null = null;

  /**
   * Initializes the element.
   */
  connectedCallback() {
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'tree');
    }

    this.addEventListener('keydown', this.handleKeyDown.bind(this));
  }

  // CrTreeBase implementation:
  /**
   * The depth of the node. This is 0 for the tree itself.
   */
  override get depth(): number {
    return 0;
  }

  override get itemsRoot(): DocumentFragment|HTMLElement {
    return this.shadowRoot!;
  }

  // These two methods should never be called for the tree itself.
  override set depth(_depth: number) {
    assertNotReached();
  }

  override setParent(_parent: CrTreeBaseElement) {
    assertNotReached();
  }

  /**
   * The selected tree item or null if none.
   */
  override get selectedItem(): CrTreeBaseElement|null {
    return this.selectedItem_ || null;
  }

  override set selectedItem(item: CrTreeBaseElement|null) {
    const oldSelectedItem = this.selectedItem_;
    if (oldSelectedItem !== item) {
      // Set the selectedItem_ before deselecting the old item since we only
      // want one change when moving between items.
      this.selectedItem_ = item;

      if (oldSelectedItem) {
        oldSelectedItem.toggleAttribute(SELECTED_ATTR, false);
      }

      if (item) {
        item.toggleAttribute(SELECTED_ATTR, true);
        if (item.id) {
          this.setAttribute('aria-activedescendant', item.id);
        }
        if (this.matches(':focus-within') || this.shadowRoot!.activeElement) {
          (item as CrTreeItemElement).rowElement.focus();
        }
      } else {
        this.removeAttribute('aria-activedescendant');
      }

      this.dispatchEvent(
          new CustomEvent('cr-tree-change', {bubbles: true, composed: true}));
    }
  }

  override addAt(child: CrTreeBaseElement, index: number) {
    super.addAt(child, index);
    // aria-owns doesn't work well for the tree because the treeitem role is
    // set on the rowElement within cr-tree-item's shadow DOM. Set the size
    // here, so the correct number of items is read.
    this.setAttribute('aria-setsize', this.items.length.toString());
  }

  /**
   * Handles keydown events on the tree and updates selection and exanding
   * of tree items.
   */
  handleKeyDown(e: KeyboardEvent) {
    let itemToSelect: CrTreeBaseElement|null = null;
    if (e.ctrlKey) {
      return;
    }

    const item = this.selectedItem;
    if (!item) {
      return;
    }

    const rtl = getComputedStyle(item).direction === 'rtl';

    switch (e.key) {
      case 'ArrowUp':
        itemToSelect = getPrevious(item);
        break;
      case 'ArrowDown':
        itemToSelect = getNext(item);
        break;
      case 'ArrowLeft':
      case 'ArrowRight':
        // Don't let back/forward keyboard shortcuts be used.
        if (!isMac && e.altKey || isMac && e.metaKey) {
          break;
        }

        if (e.key === 'ArrowLeft' && !rtl || e.key === 'ArrowRight' && rtl) {
          if (item.expanded) {
            item.expanded = false;
          } else {
            itemToSelect = item.parentItem;
          }
        } else {
          if (!item.expanded) {
            item.expanded = true;
          } else {
            itemToSelect = item.items[0] || null;
          }
        }
        break;
      case 'Home':
        itemToSelect = this.items[0] || null;
        break;
      case 'End':
        itemToSelect = this.items[this.items.length - 1] || null;
        break;
    }

    if (itemToSelect) {
      itemToSelect.toggleAttribute(SELECTED_ATTR, true);
      e.preventDefault();
    }
  }

  setIconVisibility(visibility: string) {
    this.setAttribute('icon-visibility', visibility);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-tree': CrTreeElement;
  }
}

customElements.define('cr-tree', CrTreeElement);
