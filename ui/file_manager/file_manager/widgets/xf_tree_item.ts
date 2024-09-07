// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './xf_icon.js';

import {css, customElement, html, ifDefined, property, type PropertyValues, query, state, styleMap, XfBase} from './xf_base.js';
import type {XfTree} from './xf_tree.js';
import {handleTreeSlotChange, isTreeItem, isXfTree} from './xf_tree_util.js';

/**
 * The number of pixels to indent per level.
 */
export const TREE_ITEM_INDENT = 20;

@customElement('xf-tree-item')
export class XfTreeItem extends XfBase {
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
  /** Indicate if a tree item is in renaming mode or not. */
  @property({type: Boolean, reflect: true}) renaming = false;

  /**
   * A tree item will have children if the child tree items have been inserted
   * to its default slot. Only use `mayHaveChildren` if we want the tree item
   * to appeared as having children even without the actual child tree items
   * (e.g. no DOM children). This is mainly used when we asynchronously loads
   * child tree items.
   */
  @property({type: Boolean, reflect: true, attribute: 'may-have-children'})
  mayHaveChildren = false;

  /**
   * The icon of the tree item, will be displayed before the label text.
   * The icon value should come from `ICON_TYPES`, it will be passed
   * as `type` to a <xf-icon> widget to render an icon element.
   */
  @property({type: String, reflect: true}) icon = '';
  /**
   * The icon set is an object which contains multiple base64 image data, it
   * will be passed as `iconSet` property to `<xf-icon>` widget.
   * Note: `icon` will be ignored if `iconSet` is provided.
   */
  @property({attribute: false})
  iconSet: chrome.fileManagerPrivate.IconSet|null = null;
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
   * Toggle the focusable for the item. We put the tabindex on the <li> element
   * instead of the whole <xf-tree-item> because <xf-tree-item> also includes
   * all children slots.
   *
   * We are delegate the focus to the <li> element in the shadow DOM, to make
   * sure the update is synchronous, we are operating on the DOM directly here
   * instead of updating this in the render() function.
   *
   * Note: "tabindex = -1" is also considered as "focusable" according to the
   * spec
   * https://html.spec.whatwg.org/multipage/interaction.html#the-tabindex-attribute,
   * so we need to remove the "tabindex" attribute below to make it
   * non-focusable.
   */
  toggleFocusable(focusable: boolean) {
    if (focusable) {
      this.$treeItem_.setAttribute('tabindex', '0');
    } else {
      this.$treeItem_.removeAttribute('tabindex');
    }
  }

  /**
   * Override focus() so we can manually focus the tree row element inside
   * shadow DOM.
   */
  override focus() {
    console.assert(
        !this.disabled,
        'Called focus() on a disabled XfTreeItem() isn\'t allowed');

    // Make sure this is the only focusable item in the tree before calling
    // focus().
    if (this.tree) {
      this.tree.focusedItem = this;
    }
    this.$treeItem_.focus();
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
      if (isXfTree(p)) {
        return null;
      }
      p = p.parentElement;
    }
    return p;
  }

  get tree(): XfTree|null {
    let t = this.parentElement;
    while (t && !isXfTree(t)) {
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

  /**
   * This will be called when tree item is being set as a drop target.
   */
  doDropTargetAction() {
    this.expanded = true;
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
  @query('.tree-row') private $treeRow_!: HTMLDivElement;
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
        aria-labelledby="tree-label"
        aria-selected=${this.selected}
        aria-expanded=${ifDefined(showExpandIcon ? this.expanded : undefined)}
        aria-disabled=${this.disabled}
      >
        <div class="tree-row-wrapper">
          <div
            class="tree-row"
            style=${styleMap(treeRowStyles)}
          >
            <paper-ripple></paper-ripple>
            <span class="expand-icon"></span>
            <xf-icon
              class="tree-label-icon"
              type=${ifDefined(this.iconSet ? undefined : this.icon)}
              .iconSet=${this.iconSet}
            ></xf-icon>
            <span class="tree-label" id="tree-label">${this.label || ''}</span>
            <slot name="rename"></slot>
            <slot name="trailingIcon"></slot>
          </div>
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

  /**
   * When <xf-tree-item> responds to the "contextmenu" event, the `e.target`
   * will always be the host element even if we put the focus on the inner
   * ".tree-row" element, this is because it's inside the shadow DOM. To make
   * sure the context menu shows in the correct location (when triggered by
   * keyboard), we need to expose this method to re-position the menu based on
   * the ".tree-row"'s bounding box. This method will be invoked by
   * `ContextMenuHandler`.
   */
  getRectForContextMenu(): DOMRect {
    return this.$treeRow_.getBoundingClientRect();
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

    const newItems = new Set(this.items_);
    if (this.tree) {
      handleTreeSlotChange(this.tree, oldItems, newItems);
    }

    if (!updateScheduled) {
      // Explicitly trigger an update because render() relies on hasChildren(),
      // which relies on `this.items_`.
      this.requestUpdate();
    }
  }

  override firstUpdated() {
    this.updateLevel_();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    // For browser test use only.
    this.setAttribute('has-children', String(this.items_.length > 0));
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
  return css`
    :host {
      --xf-tree-item-indent: ${TREE_ITEM_INDENT};
      display: block;
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

    /* We need this layer to make sure there's no gap between tree items, so
    when we drag items onto the tree items, it won't activate the parent tree
    item unexpectedly. */
    .tree-row-wrapper {
      cursor: pointer;
      padding: 4px;
    }

    .tree-row {
      align-items: center;
      border-inline-start-width: 0 !important;
      border-radius: 20px;
      box-sizing: border-box;
      color: var(--cros-sys-on_surface);
      display: flex;
      height: 40px;
      padding-inline-end: 12px;
      position: relative;
      user-select: none;
      white-space: nowrap;
    }

    :host(:not([selected]):not([disabled]):not([renaming]):not(:focus))
        .tree-row:hover {
      background-color: var(--cros-sys-hover_on_subtle);
    }

    :host([selected]) .tree-row {
      background-color: var(--cros-sys-primary);
      color: var(--cros-sys-on_primary);
    }

    :host([disabled]) .tree-row {
      color: var(--cros-sys-disabled);
      pointer-events: none;
    }

    :host-context(.focus-outline-visible):host(:focus) .tree-row {
      outline: 2px solid var(--cros-sys-focus_ring);
      outline-offset: 2px;
      z-index: 2;
    }

    :host-context(.pointer-active):host(:not([selected]):not([disabled]):not([renaming]):not(:focus))
        .tree-row:not(:hover):active {
      background-color: var(--cros-sys-hover_on_subtle);
    }

    :host-context(.pointer-active) .tree-row:not(:active) {
      cursor: default;
    }

    :host-context(.pointer-active):host(:not([selected]):not([disabled]):not([renaming]):not(:focus))
        .tree-row:not(:active):hover {
      background-color: unset;
    }

    :host-context(html.drag-drop-active):host(.denies) .tree-row {
      background-color: var(--cros-sys-error_container);
      color: var(--cros-sys-on_error_container);
    }

    :host-context(html.drag-drop-active):host(.accepts) .tree-row {
      background-color: var(--cros-sys-hover_on_subtle);
    }

    :host-context(html.drag-drop-active):host(.accepts[selected]) .tree-row {
      background-color: var(--cros-sys-primary);
    }

    .expand-icon {
      -webkit-mask-image: url(../foreground/images/files/ui/sort_desc.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: currentColor;
      flex: none;
      height: 20px;
      margin-inline-start: 8px;
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
      --xf-icon-color: var(--cros-sys-on_surface);
      flex: none;
    }

    :host([selected]) .tree-label-icon {
      --xf-icon-color: var(--cros-sys-on_primary)
    }

    :host([disabled]) .tree-label-icon {
      --xf-icon-color: var(--cros-sys-disabled);
    }

    .tree-label {
      display: block;
      flex: auto;
      font: var(--cros-button-2-font);
      margin-inline-end: 2px;
      margin-inline-start: 8px;
      min-width: 0;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: pre;
    }

    /** input is attached by DirectoryTreeNamingController. */
    slot[name="rename"]::slotted(input) {
      background-color: var(--cros-sys-app_base);
      border-radius: 4px;
      border: none;
      color: var(--cros-sys-on_surface);
      display: none;
      font: var(--cros-body-2-font);
      height: 20px;
      width: 100%;
      margin: 0 10px;
      outline: 2px solid var(--cros-sys-focus_ring);
      overflow: hidden;
      padding: 1px 8px;
    }

    :host([renaming]) slot[name="rename"]::slotted(input) {
      display: block;
    }

    :host([renaming]) .tree-label {
      display: none;
    }

    :host([selected]) slot[name="rename"]::slotted(input) {
      outline: 2px solid var(--cros-sys-inverse_primary);
    }

    paper-ripple {
      border-radius: 20px;
      color: var(--cros-sys-ripple_primary);
    }

    /* We need to ensure that even empty labels take up space. */
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

    /* Trailing icon styles. */
    slot[name="trailingIcon"]::slotted(.align-right-icon) {
      --ink-color: var(--cros-sys-ripple_neutral_on_subtle);
      --iron-icon-height: 20px;
      --iron-icon-width: 20px;
      -ripple-opacity: 100%;
      border: none;
      border-radius: 20px;
      box-sizing: border-box;
      height: 40px;
      position: relative;
      right: -12px; /* Same as padding inline end of tree row. */
      width: 40px;
      z-index: 1;
    }

    :host-context([dir="rtl"]) slot[name="trailingIcon"]::slotted(.align-right-icon) {
      left: -12px; /* Same as padding inline end of tree row. */
      right: unset;
    }

    slot[name="trailingIcon"]::slotted(.external-link-icon iron-icon) {
      padding: 6px;
    }

    slot[name="trailingIcon"]::slotted(.root-eject) {
      --text-color: currentColor;
      --hover-bg-color: none;
      --ripple-opacity: 1;
      min-width: 32px;
      padding: 0;
    }

    slot[name="trailingIcon"]::slotted(.root-eject:focus) {
      outline: 2px solid var(--cros-sys-focus_ring);
      outline-offset: 2px;
    }

    :host([selected]) slot[name="trailingIcon"]::slotted(.root-eject:focus) {
      outline: 2px solid var(--cros-sys-inverse_primary);
    }

    slot[name="trailingIcon"]::slotted(.root-eject:active) {
      --ink-color: var(--cros-sys-ripple_neutral_on_subtle);
    }

    :host([selected]) slot[name="trailingIcon"]::slotted(.root-eject:active) {
      --ink-color: var(--cros-sys-ripple_neutral_on_prominent);
    }
  `;
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
