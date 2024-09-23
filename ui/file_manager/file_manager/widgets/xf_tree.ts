// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {isRTL} from 'chrome://resources/ash/common/util.js';

import {css, customElement, html, query, state, XfBase} from './xf_base.js';
import type {XfTreeItem} from './xf_tree_item.js';
import {type TreeItemCollapsedEvent} from './xf_tree_item.js';
import {handleTreeSlotChange, isTreeItem} from './xf_tree_util.js';

/**
 * <xf-tree> is the container of the <xf-tree-item> elements. An example
 * DOM structure is like this:
 *
 * <xf-tree>
 *   <xf-tree-item>
 *     <xf-tree-item></xf-tree-item>
 *   </xf-tree-item>
 *   <xf-tree-item></xf-tree-item>
 * </xf-tree>
 *
 * The selection and focus of <xf-tree-item> is controlled in <xf-tree>,
 * this is because we need to make sure only one item is being selected or
 * focused.
 *
 */
@customElement('xf-tree')
export class XfTree extends XfBase {
  static get events() {
    return {
      /** Triggers when a tree item has been selected. */
      TREE_SELECTION_CHANGED: 'tree_selection_changed',
    } as const;
  }

  /** Return the selected tree item, could be null. */
  get selectedItem(): XfTreeItem|null {
    return this.selectedItem_;
  }
  set selectedItem(item: XfTreeItem|null) {
    this.selectItem_(item);
  }

  /** Return the focused tree item, could be null. */
  get focusedItem(): XfTreeItem|null {
    return this.focusedItem_;
  }
  set focusedItem(item: XfTreeItem|null) {
    this.makeItemFocusable_(item);
  }

  /** The child tree items. */
  get items(): XfTreeItem[] {
    return this.items_;
  }

  /** The child tree items which can be tabbed/focused into. */
  get tabbableItems(): XfTreeItem[] {
    return this.items_.filter(item => !item.disabled);
  }

  /** The default unnamed slot to let consumer pass children tree items. */
  @query('slot') private $childrenSlot_!: HTMLSlotElement;

  /** The child tree items. */
  private items_: XfTreeItem[] = [];
  /**
   * Maintain these in the tree level so we can make sure at most one tree item
   * can be selected/focused.
   */
  private selectedItem_: XfTreeItem|null = null;
  private focusedItem_: XfTreeItem|null = null;

  /**
   * Value to set aria-setsize, which is the number of the top level child tree
   * items.
   */
  @state() private ariaSetSize_ = 0;

  static override get styles() {
    return getCSS();
  }

  /**
   * The <xf-tree> itself is not focusable, it will delegate the focus down to
   * its `focusedItem_`.
   *
   * Note: previously we use `delegatesFocus: true` in the shadowRootOptions,
   * but it triggers weird behavior b/320580121, hence the override here.
   */
  override focus() {
    if (this.focusedItem_) {
      this.focusedItem_.focus();
    }
  }

  override render() {
    return html`
      <ul
        class="tree"
        role="tree"
        aria-setsize=${this.ariaSetSize_}
        @tree_item_collapsed=${this.onTreeItemCollapsed_}
      >
        <slot @slotchange=${this.onSlotChanged_}></slot>
      </ul>
    `;
  }

  override connectedCallback(): void {
    super.connectedCallback();
    // Binding all these events at the host element level because the blank
    // space of the tree doesn't belong to the root <ul> element.
    this.addEventListener('contextmenu', this.onHostContextMenu_.bind(this));
    this.addEventListener('click', this.onHostClicked_.bind(this));
    this.addEventListener('dblclick', this.onHostDblClicked_.bind(this));
    this.addEventListener('mousedown', this.onHostMouseDown_.bind(this));
    this.addEventListener('keydown', this.onHostKeyDown_.bind(this));
  }

  private onSlotChanged_() {
    const oldItems = new Set(this.items_);
    // Update `items_` every time when the children slot changes (e.g.
    // add/remove).
    this.items_ = this.$childrenSlot_.assignedElements().filter(isTreeItem);
    this.ariaSetSize_ = this.tabbableItems.length;

    const newItems = new Set(this.items_);
    handleTreeSlotChange(this, oldItems, newItems);
  }

  /**
   * Handles the collapse event of the tree item.
   */
  private onTreeItemCollapsed_(e: TreeItemCollapsedEvent) {
    const treeItem = e.detail.item;
    // If the currently focused tree item (`oldFocusedItem`) is a descent of
    // another tree item (`treeItem`) which is going to be collapsed, we need to
    // mark the ancestor tree item (`this`) as focused.
    if (this.focusedItem_ !== treeItem) {
      const oldFocusedItem = this.focusedItem_;
      if (oldFocusedItem && treeItem.contains(oldFocusedItem)) {
        this.makeItemFocusable_(treeItem);
      }
    }
  }

  /** Called when the user clicks within the host element. */
  private onHostClicked_(e: MouseEvent) {
    // Mouse right click won't trigger click event, so this check is not
    // necessary in real scenario. This is mainly for the browser test because
    // waitAndRightClickEvent will actually trigger a click event with button=2.
    if (e.button === 2) {
      return;
    }

    // Stop if the the click target is not a tree item.
    const treeItem = e.target as XfTreeItem;
    if (treeItem && !isTreeItem(treeItem)) {
      // Clicking the non tree item area should focus the whole tree, which will
      // delegate the focus to the currently focusable child tree item.
      this.focus();
      return;
    }

    if (treeItem.disabled) {
      e.stopImmediatePropagation();
      e.preventDefault();
      return;
    }

    // Use composed path to know which element inside the shadow root
    // has been clicked.
    const innerClickTarget = e.composedPath()[0] as HTMLElement;
    if (innerClickTarget.className === 'expand-icon') {
      treeItem.expanded = !treeItem.expanded;
    } else {
      treeItem.selected = true;
    }
    treeItem.focus();
  }

  /** Called when the user double clicks within the host element. */
  private onHostDblClicked_(e: MouseEvent) {
    // Stop if the the click target is not a tree item.
    const treeItem = e.target as XfTreeItem;
    if (treeItem && !isTreeItem(treeItem)) {
      // Double clicking the non tree item area should focus the whole tree,
      // which will delegate the focus to the currently focusable child tree
      // item.
      this.focus();
      return;
    }

    if (treeItem.disabled) {
      e.stopImmediatePropagation();
      e.preventDefault();
      return;
    }

    // Use composed path to know which element inside the shadow root
    // has been clicked.
    const innerClickTarget = e.composedPath()[0] as HTMLElement;
    if (innerClickTarget.className !== 'expand-icon' &&
        treeItem.hasChildren()) {
      treeItem.expanded = !treeItem.expanded;
      treeItem.focus();
    }
  }

  /** Called when mouse down event happens within the host element. */
  private onHostMouseDown_(e: MouseEvent) {
    // Only handle the right click here, left click is handled by the click
    // handler above.
    if (e.button !== 2) {
      return;
    }

    // Stop if the the click target is not a tree item.
    const treeItem = e.target as XfTreeItem;
    if (treeItem && !isTreeItem(treeItem)) {
      // Right clicking the non tree item area should focus the whole tree,
      // which will delegate the focus to the currently focusable child tree
      // item.
      this.focus();
      return;
    }

    if (treeItem.disabled) {
      e.stopImmediatePropagation();
      e.preventDefault();
      return;
    }

    treeItem.focus();
  }

  /** Called when a context menu event happens within the host element. */
  private onHostContextMenu_(e: MouseEvent) {
    // Delegate the tree level contextmenu event to the focused child tree item.
    // Note: tree item contextmenu event will never arrive here because the
    // event listener registered in ContextMenuHandler stops propagation after
    // showing the context menu. So the handler here is only for right clicking
    // on the blank space area (e.g. outside the root <ul> element).
    if (this.focusedItem_) {
      const domRect = this.focusedItem_.getRectForContextMenu();
      // Calculate the center point of the tree item, so <xf-tree-item> knows
      // where to show the context menu pop-up.
      const x = domRect.x + (domRect.width / 2);
      const y = domRect.y + (domRect.height / 2);
      this.focusedItem_.dispatchEvent(
          new PointerEvent(e.type, {...e, clientX: x, clientY: y}));
    }
  }

  /**
   * Handle the keydown within the host element, this mainly handles the
   * navigation and the selection with the keyboard.
   */
  private onHostKeyDown_(e: KeyboardEvent) {
    if (e.ctrlKey) {
      return;
    }
    // We allow repeated keydown (e.g. hold the key without releasing to trigger
    // event multiple times) only for ArrowUp/ArrowDown, so users can use hold
    // arrow up/down to quickly navigate to the tree items far away.
    const allowRepeat = e.key === 'ArrowUp' || e.key === 'ArrowDown';
    if (e.repeat && !allowRepeat) {
      return;
    }

    if (!this.focusedItem_) {
      return;
    }

    if (this.tabbableItems.length === 0) {
      return;
    }

    let itemToFocus: XfTreeItem|null|undefined = null;
    switch (e.key) {
      case 'Enter':
      case ' ':
        this.selectItem_(this.focusedItem_);
        break;
      case 'ArrowUp':
        itemToFocus = this.getPreviousItem_(this.focusedItem_);
        break;
      case 'ArrowDown':
        itemToFocus = this.getNextItem_(this.focusedItem_);
        break;
      case 'ArrowLeft':
      case 'ArrowRight':
        // Don't let back/forward keyboard shortcuts be used.
        if (e.altKey) {
          break;
        }

        const expandKey = isRTL() ? 'ArrowLeft' : 'ArrowRight';
        if (e.key === expandKey) {
          if (this.focusedItem_.hasChildren() && !this.focusedItem_.expanded) {
            this.focusedItem_.expanded = true;
          } else {
            itemToFocus = this.focusedItem_.tabbableItems[0];
          }
        } else {
          if (this.focusedItem_.expanded) {
            this.focusedItem_.expanded = false;
          } else {
            itemToFocus = this.focusedItem_.parentItem;
          }
        }
        break;
      case 'Home':
        itemToFocus = this.tabbableItems[0];
        break;
      case 'End':
        itemToFocus = this.tabbableItems[this.tabbableItems.length - 1];
        break;
    }

    if (itemToFocus) {
      itemToFocus.focus();
      e.preventDefault();
    }
  }

  /**
   * Helper function that returns the next tabbable tree item.
   */
  private getNextItem_(item: XfTreeItem): XfTreeItem|null {
    if (item.expanded && item.tabbableItems.length > 0) {
      return item.tabbableItems[0]!;
    }

    return this.getNextHelper_(item);
  }

  /**
   * Another helper function that returns the next tabbable tree item.
   */
  private getNextHelper_(item: XfTreeItem|null): XfTreeItem|null {
    if (!item) {
      return null;
    }

    const nextSibling = item.nextElementSibling as XfTreeItem | null;
    if (nextSibling) {
      if (nextSibling.disabled) {
        return this.getNextHelper_(nextSibling);
      }
      return nextSibling;
    }
    return this.getNextHelper_(item.parentItem);
  }

  /**
   * Helper function that returns the previous tabbable tree item.
   */
  private getPreviousItem_(item: XfTreeItem): XfTreeItem|null {
    let previousSibling = item.previousElementSibling as XfTreeItem | null;
    while (previousSibling && previousSibling.disabled) {
      previousSibling =
          previousSibling.previousElementSibling as XfTreeItem | null;
    }
    if (previousSibling) {
      return this.getLastHelper_(previousSibling);
    }
    return item.parentItem;
  }

  /**
   * Helper function that returns the last tabbable tree item in the subtree.
   */
  private getLastHelper_(item: XfTreeItem|null): XfTreeItem|null {
    if (!item) {
      return null;
    }
    if (item.expanded && item.tabbableItems.length > 0) {
      const lastChild = item.tabbableItems[item.tabbableItems.length - 1]!;
      return this.getLastHelper_(lastChild);
    }
    return item;
  }

  /**
   * Make `itemToSelect` become the selected item in the tree, this will
   * also unselect the previously selected tree item to make sure at most
   * one tree item is selected in the tree.
   */
  private selectItem_(itemToSelect: XfTreeItem|null) {
    const previousSelectedItem = this.selectedItem_;
    if (itemToSelect === previousSelectedItem) {
      return;
    }
    if (previousSelectedItem) {
      previousSelectedItem.selected = false;
    }
    this.selectedItem_ = itemToSelect;
    if (this.selectedItem_) {
      this.selectedItem_.selected = true;
      // When tree item gets selected programmatically (e.g. not through
      // mouse/keyboard), there might be other elements on the page which have
      // the focus, we don't want to steal the focus, so all we do here is to
      // make the item focusable.
      this.makeItemFocusable_(this.selectedItem_);
    }
    const selectionChangeEvent: TreeSelectedChangedEvent =
        new CustomEvent(XfTree.events.TREE_SELECTION_CHANGED, {
          bubbles: true,
          composed: true,
          detail: {
            previousSelectedItem,
            selectedItem: this.selectedItem,
          },
        });
    this.dispatchEvent(selectionChangeEvent);
  }

  /**
   * Make `itemToFocus` become the focusable, this will also make the previously
   * focused item non-focusable so we can make sure only 1 tree item is
   * focusable, this is essential for "delegatesFocus" to work.
   *
   * Note: this method only make the item to be focusable, it won't actually
   * focus the item, we need to call `.focus()` after to focus it.
   */
  private makeItemFocusable_(itemToFocus: XfTreeItem|null) {
    const previousFocusedItem = this.focusedItem_;
    if (previousFocusedItem === itemToFocus) {
      return;
    }
    if (previousFocusedItem) {
      previousFocusedItem.toggleFocusable(false);
    }
    this.focusedItem_ = itemToFocus;
    if (this.focusedItem_) {
      this.focusedItem_.toggleFocusable(true);
    }
  }
}

function getCSS() {
  return css`
    :host {
      display: block;
    }

    ul {
      list-style: none;
      margin: 0;
      padding: 0;
    }
  `;
}

/** Type of the tree item selection custom event. */
export type TreeSelectedChangedEvent = CustomEvent<{
  /** The tree item which has been selected previously. */
  previousSelectedItem: XfTreeItem | null,
  /** The tree item which has been selected now. */
  selectedItem: XfTreeItem | null,
}>;

declare global {
  interface HTMLElementEventMap {
    [XfTree.events.TREE_SELECTION_CHANGED]: TreeSelectedChangedEvent;
  }

  interface HTMLElementTagNameMap {
    'xf-tree': XfTree;
  }
}
