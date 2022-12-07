// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';

import {addCSSPrefixSelector} from '../common/js/dom_utils.js';

import {css, customElement, html, ifDefined, property, PropertyValues, query, state, styleMap, XfBase} from './xf_base.js';
import type {XfTree} from './xf_tree.js';
import {isTree, isTreeItem} from './xf_tree_util.js';

/**
 * The number of pixels to indent per level.
 */
export const TREE_ITEM_INDENT = 20;

@customElement('xf-tree-item')
export class XfTreeItem extends XfBase {
  /**
   * Override the tabIndex because we need to assign it to the <li> element
   * instead of the host element.
   */
  @property({attribute: false}) override tabIndex: number = -1;

  /**
   * `separator` attribute will show a top border for the tree item. It's
   * mainly used to identify this tree item is a start of the new section.
   */
  @property({type: Boolean, reflect: true}) separator = false;
  /**
   * Indicate if a tree item is disabled or not. Disabled tree item will have
   * a grey out color, can't be selected, can't get focus. It can still have
   * children, but it can't be expanded, and the expand icon will be hidden.
   */
  @property({type: Boolean, reflect: true}) disabled = false;
  /** Indicate if a tree item has been selected or not. */
  @property({type: Boolean, reflect: true}) selected = false;
  /** Indicate if a tree item has been expanded or not. */
  @property({type: Boolean, reflect: true}) expanded = false;

  /**
   * A tree item will have children if the child tree items have been inserted
   * to its default slot. Only use `mayHaveChildren` if we want the tree item
   * to appeared as having children even without the actual child tree items
   * (e.g. no DOM children). This is mainly used when we asynchronously loads
   * child tree items.
   */
  @property({type: Boolean, reflect: true, attribute: 'may-have-children'})
  mayHaveChildren = false;

  /** The icon of the tree item, will be displayed before the label text. */
  @property({type: String, reflect: true, attribute: 'icon'}) icon = '';
  /** The label text of the tree item. */
  @property({type: String, reflect: true}) label = '';

  static get events() {
    return {
      /** Triggers when a tree item has been expanded. */
      TREE_ITEM_EXPANDED: 'tree_item_expanded',
      /** Triggers when a tree item has been collapsed. */
      TREE_ITEM_COLLAPSED: 'tree_item_collapsed',
    } as const;
  }

  /**
   * Override to focus the inner <li> instead of the host element.
   * We use tabIndex to control if a tree item can be focused or not, need
   * to set it to 0 before focusing the item.
   */
  override focus() {
    console.assert(
        !this.disabled,
        'Called focus() on a disabled XfTreeItem() isn\'t allowed');

    this.tabIndex = 0;
    this.$treeItem_.focus();
  }

  /**
   * Override to blur the inner <li> instead of the host element.
   * We use tabIndex to control if a tree item can be focused or not, need
   * to set it to -1 after blurring the item.
   */
  override blur() {
    console.assert(
        !this.disabled,
        'Called blur() on a disabled XfTreeItem() isn\'t allowed');

    this.tabIndex = -1;
    this.$treeItem_.blur();
  }

  /** The level of the tree item, starting from 1. */
  get level(): number {
    return this.level_;
  }

  /** The child tree items. */
  get items(): XfTreeItem[] {
    return this.items_;
  }

  /** The child tree items which can be tabbed. */
  get tabbableItems(): XfTreeItem[] {
    return this.items_.filter(item => !item.disabled);
  }

  hasChildren(): boolean {
    return this.mayHaveChildren || this.items_.length > 0;
  }

  /**
   * Return the parent XfTreeItem if there is one, for top level XfTreeItem
   * which doesn't have parent XfTreeItem, return null.
   */
  get parentItem(): XfTreeItem|null {
    let p = this.parentElement;
    while (p) {
      if (isTreeItem(p)) {
        return p;
      }
      if (isTree(p)) {
        return null;
      }
      p = p.parentElement;
    }
    return p;
  }

  get tree(): XfTree|null {
    let t = this.parentElement;
    while (t && !isTree(t)) {
      t = t.parentElement;
    }
    return t;
  }

  /**
   * Expands all parent items.
   */
  reveal() {
    let pi = this.parentItem;
    while (pi) {
      pi.expanded = true;
      pi = pi.parentItem;
    }
  }

  static override get styles() {
    return getCSS();
  }

  /**
   * Indicate the level of this tree item, we use it to calculate the padding
   * indentation. Note: "aria-level" can be calculated by DOM structure so
   * no need to provide it explicitly.
   */
  @state() private level_ = 1;

  @query('li') private $treeItem_!: HTMLLIElement;
  @query('slot:not([name])') private $childrenSlot_!: HTMLSlotElement;

  /** The child tree items. */
  private items_: XfTreeItem[] = [];

  override render() {
    const showExpandIcon = this.hasChildren() && !this.disabled;
    const treeRowStyles = {
      paddingInlineStart:
          `max(0px, calc(var(--xf-tree-item-indent) * ${this.level_ - 1}px))`,
    };

    return html`
      <li
        class="tree-item"
        role="treeitem"
        tabindex=${this.tabIndex}
        aria-labelledby="tree-label"
        aria-selected=${this.selected}
        aria-expanded=${ifDefined(showExpandIcon ? this.expanded : undefined)}
        aria-disabled=${this.disabled}
      >
        <div
          class="tree-row"
          style=${styleMap(treeRowStyles)}
        >
          <paper-ripple></paper-ripple>
          <span class="expand-icon"></span>
          <span
            class="tree-label-icon"
            tree-icon-type=${this.icon}
          ></span>
          <span
            class="tree-label"
            id="tree-label"
          >${this.label || ''}</span>
          <slot name="trailingIcon"></slot>
        </div>
        <ul
          class="tree-children"
          role="group"
        >
          <slot @slotchange=${this.onSlotChanged_}></slot>
        </ul>
      </li>
    `;
  }

  override connectedCallback() {
    super.connectedCallback();
    if (!this.tree) {
      throw new Error(
          '<xf-tree-item> can not be used without a parent <xf-tree>');
    }
  }

  private onSlotChanged_() {
    const oldItems = new Set(this.items_);
    // Update `items_` every time when the children slot changes (e.g.
    // add/remove).
    this.items_ = this.$childrenSlot_.assignedElements().filter(isTreeItem);

    let updateScheduled = false;

    // If an expanded item's last children is deleted, update expanded property.
    if (this.items_.length === 0 && this.expanded) {
      this.expanded = false;
      updateScheduled = true;
    }

    if (this.tree?.selectedItem) {
      const newItems = new Set(this.items_);
      if (oldItems.has(this.tree.selectedItem) &&
          !newItems.has(this.tree.selectedItem)) {
        // If the currently selected item exists in `oldItems` but not in
        // `newItems`, it means it's being removed from the children slot,
        // we need to select the parent node of the removed item (i.e. `this`).
        this.selected = true;
        updateScheduled = true;
      }
    }

    if (!updateScheduled) {
      // Explicitly trigger an update because render() relies on hasChildren().
      this.requestUpdate();
    }
  }

  override firstUpdated() {
    this.updateLevel_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('expanded')) {
      this.onExpandChanged_();
    }
    if (changedProperties.has('selected')) {
      this.onSelectedChanged_();
    }
  }

  private onExpandChanged_() {
    if (this.expanded) {
      const expandedEvent: TreeItemExpandedEvent =
          new CustomEvent(XfTreeItem.events.TREE_ITEM_EXPANDED, {
            bubbles: true,
            composed: true,
            detail: {item: this},
          });
      this.dispatchEvent(expandedEvent);
    } else {
      const collapseEvent: TreeItemCollapsedEvent =
          new CustomEvent(XfTreeItem.events.TREE_ITEM_COLLAPSED, {
            bubbles: true,
            composed: true,
            detail: {item: this},
          });
      this.dispatchEvent(collapseEvent);
    }
  }

  private onSelectedChanged_() {
    const tree = this.tree;
    if (this.selected) {
      this.reveal();
      if (tree) {
        tree.selectedItem = this;
      }
    } else {
      if (tree && tree.selectedItem === this) {
        tree.selectedItem = null;
      }
    }
  }

  /** Update the level of the tree item by traversing upwards. */
  private updateLevel_() {
    // Traverse upwards to determine the level.
    let level = 0;
    let current: XfTreeItem|null = this;
    while (current) {
      current = current.parentItem;
      level++;
    }
    this.level_ = level;
  }
}

function getCSS() {
  const commonCSS = css`
    :host {
      --xf-tree-item-indent: ${TREE_ITEM_INDENT};
    }

    ul {
      list-style: none;
      margin: 0;
      outline: none;
      padding: 0;
    }

    li {
      display: block;
    }

    li:focus-visible {
      outline: none;
    }

    :host([separator])::before {
      border-bottom: 1px solid var(--cros-separator-color);
      content: '';
      display: block;
      margin: 8px 0;
      width: 100%;
    }

    .tree-row {
      align-items: center;
      border-inline-start-width: 0 !important;
      box-sizing: border-box;
      cursor: pointer;
      display: flex;
      position: relative;
      user-select: none;
      white-space: nowrap;
    }

    li:focus-visible .tree-row {
      z-index: 2;
    }

    :host([disabled]) .tree-row {
      pointer-events: none;
    }

    .expand-icon {
      -webkit-mask-image: url(../foreground/images/files/ui/sort_desc.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: currentColor;
      flex: none;
      height: 20px;
      position: relative;
      transform: rotate(-90deg);
      transition: all 150ms;
      visibility: hidden;
      width: 20px;
    }

    li[aria-expanded] .expand-icon {
      visibility: visible;
    }

    :host-context(html[dir=rtl]) .expand-icon {
      transform: rotate(90deg);
    }

    :host([expanded]) .expand-icon {
      transform: rotate(0);
    }

    .tree-label-icon {
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: var(--cros-icon-color-primary);
      background-image: none;
      flex: none;
      height: 20px;
      position: relative;
      width: 20px;
    }

    .tree-label {
      display: block;
      flex: auto;
      font-weight: 500;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: pre;
    }

    /* We need to ensure that even empty labels take up space */
    .tree-label:empty::after {
      content: ' ';
      white-space: pre;
    }

    .tree-children {
      display: none;
    }

    :host([expanded]) .tree-children {
      display: block;
    }

    slot[name="trailingIcon"]::slotted(*) {
      height: 20px;
      margin: 0;
      width: 20px;
    }
  `;

  const legacyStyle = css`
    :host {
      --xf-tree-item-indent: 22;
    }

    .tree-row {
      border: 2px solid transparent;
      border-radius: 0 20px 20px 0;
      color: var(--cros-text-color-primary);
      height: 32px;
      margin-inline-end: 6px;
      padding: 4px 0;
    }

    :host-context(html[dir=rtl]) .tree-row {
      border-radius: 20px 0 0 20px;
    }

    :host(:not([selected]):not([disabled])) .tree-row:hover {
      background-color: var(--cros-ripple-color);
    }

    :host([selected]) .tree-row {
      background-color: var(--cros-highlight-color);
      color: var(--cros-text-color-selection);
    }

    :host([disabled]) .tree-row {
      opacity: var(--cros-disabled-opacity);
    }

    li:focus-visible .tree-row {
      border: 2px solid var(--cros-focus-ring-color);
    }

    .expand-icon {
      padding: 6px;
    }

    .tree-label-icon {
      left: -4px;
      right: -4px;
    }

    .tree-label {
      margin: 0 12px;
    }

    paper-ripple {
      display: none;
    }
  `;

  const refresh23Style = css`
    .tree-row {
      border-radius: 20px;
      color: var(--cros-sys-on_surface);
      height: 40px;
      margin: 8px 0;
    }

    :host(:not([selected]):not([disabled])) .tree-row:hover {
      background-color: var(--cros-sys-hover_on_subtle);
    }

    :host([selected]) .tree-row {
      background-color: var(--cros-sys-primary);
      color: var(--cros-sys-on_primary);
    }

    :host([disabled]) .tree-row {
      color: var(--cros-sys-disabled);
    }

    li:focus-visible .tree-row {
      outline: 2px solid var(--cros-sys-focus_ring);
      outline-offset: 2px;
    }

    .expand-icon {
      margin-inline-start: 28px;
    }

    .tree-label {
      margin-inline-start: 8px;
    }

    paper-ripple {
      color: var(--cros-sys-ripple_primary);
    }
  `;

  return [
    commonCSS,
    addCSSPrefixSelector(legacyStyle, '[theme="legacy"]'),
    addCSSPrefixSelector(refresh23Style, '[theme="refresh23"]'),
  ];
}

/** Type of the tree item expanded custom event. */
export type TreeItemExpandedEvent = CustomEvent<{
  /** The tree item which has been expanded. */
  item: XfTreeItem,
}>;
/** Type of the tree item collapsed custom event. */
export type TreeItemCollapsedEvent = CustomEvent<{
  /** The tree item which has been collapsed. */
  item: XfTreeItem,
}>;

declare global {
  interface HTMLElementEventMap {
    [XfTreeItem.events.TREE_ITEM_EXPANDED]: TreeItemExpandedEvent;
    [XfTreeItem.events.TREE_ITEM_COLLAPSED]: TreeItemCollapsedEvent;
  }

  interface HTMLElementTagNameMap {
    'xf-tree-item': XfTreeItem;
  }
}
