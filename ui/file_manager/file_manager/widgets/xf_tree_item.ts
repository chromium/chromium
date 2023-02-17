// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import './xf_icon.js';

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
  /** Indicate if a tree item is in editing mode (rename) or not. */
  @property({type: Boolean, reflect: true}) editing = false;

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
   * The icon value should come from `constants.ICON_TYPES`, it will be passed
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
      /** Triggers when a tree item's label has been renamed. */
      TREE_ITEM_RENAMED: 'tree_item_renamed',
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
  @query('.rename') private $renameInput_?: HTMLInputElement;
  @query('slot:not([name])') private $childrenSlot_!: HTMLSlotElement;

  /** The child tree items. */
  private items_: XfTreeItem[] = [];

  /** Indicate if we should commit the rename on input blur or not. */
  private shouldRenameOnBlur_ = true;

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
          <xf-icon
            class="tree-label-icon"
            type=${ifDefined(this.iconSet ? undefined : this.icon)}
            .iconSet=${this.iconSet}
          ></xf-icon>
          ${this.renderTreeLabel()}
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

  private renderTreeLabel() {
    if (this.editing) {
      // Stop propagation of some events to prevent them being captured by
      // tree when the tree item is in editing mode.
      return html`
        <input
          class="rename"
          type="text"
          spellcheck="false"
          .value=${this.label}
          @click=${(e: MouseEvent) => e.stopPropagation()}
          @dblclick=${(e: MouseEvent) => e.stopPropagation()}
          @mouseup=${(e: MouseEvent) => e.stopPropagation()}
          @mousedown=${(e: MouseEvent) => e.stopPropagation()}
          @blur=${this.onRenameInputBlur_}
          @keydown=${this.onRenameInputKeydown_}
        />
      `;
    }
    return html`
    <span
      class="tree-label"
      id="tree-label"
    >${this.label || ''}</span>`;
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
        // we need to mark the selected item to null.
        this.tree.selectedItem = null;
      }
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
    if (changedProperties.has('expanded')) {
      this.onExpandChanged_();
    }
    if (changedProperties.has('selected')) {
      this.onSelectedChanged_();
    }
    if (changedProperties.has('editing')) {
      this.onEditingChanged_();
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

  private onEditingChanged_() {
    if (this.editing) {
      this.$renameInput_?.focus();
      this.$renameInput_?.select();
    }
  }

  private onRenameInputKeydown_(e: KeyboardEvent) {
    // Make sure that the tree does not handle the key.
    e.stopPropagation();

    if (e.repeat) {
      return;
    }

    // Calling this.focus blurs the input which will make the tree item
    // non editable.
    switch (e.key) {
      case 'Escape':
        // By default blur() will trigger the rename, but when ESC is pressed
        // we don't want the blur() (triggered by focus() below) to commit
        // the rename.
        this.shouldRenameOnBlur_ = false;
        this.focus();
        e.preventDefault();
        break;
      case 'Enter':
        // focus() will trigger blur() for the rename input which will commit
        // the rename.
        this.focus();
        e.preventDefault();
        break;
    }
  }

  private onRenameInputBlur_() {
    this.editing = false;
    if (this.shouldRenameOnBlur_) {
      this.commitRename_(this.$renameInput_?.value || '');
    } else {
      this.shouldRenameOnBlur_ = true;
    }
  }

  private commitRename_(newName: string) {
    const isEmpty = newName.trim() === '';
    const isChanged = newName !== this.label;
    if (isEmpty || !isChanged) {
      return;
    }
    const oldLabel = this.label;
    this.label = newName;
    const renameEvent: TreeItemRenamedEvent =
        new CustomEvent(XfTreeItem.events.TREE_ITEM_RENAMED, {
          bubbles: true,
          composed: true,
          detail: {item: this, oldLabel, newLabel: newName},
        });
    this.dispatchEvent(renameEvent);
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
  const legacyStyle = css`
    :host {
      --xf-tree-item-indent: 22;
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
      border-radius: 0 20px 20px 0;
      border: 2px solid transparent;
      box-sizing: border-box;
      color: var(--cros-text-color-primary);
      cursor: pointer;
      display: flex;
      height: 32px;
      margin-inline-end: 6px;
      padding: 4px 0;
      position: relative;
      user-select: none;
      white-space: nowrap;
    }

    :host-context(html[dir=rtl]) .tree-row {
      border-radius: 20px 0 0 20px;
    }

    :host(:not([selected]):not([disabled]):not([editing]))
        li:not(:focus-visible) .tree-row:hover {
      background-color: var(--cros-ripple-color);
    }

    :host([selected]) .tree-row {
      background-color: var(--cros-highlight-color);
      color: var(--cros-text-color-selection);
    }

    :host([disabled]) .tree-row {
      opacity: var(--cros-disabled-opacity);
      pointer-events: none;
    }

    :host-context(.pointer-active):host(:not([selected]):not([disabled]):not([editing]))
        li:not(:focus-visible) .tree-row:not(:hover):active {
      background-color: var(--cros-ripple-color);
    }

    li:focus-visible .tree-row {
      border: 2px solid var(--cros-focus-ring-color);
      z-index: 2;
    }

    :host-context(.pointer-active) .tree-row:not(:active) {
      cursor: default;
    }

    :host-context(.pointer-active):host(:not([selected]):not([disabled]):not([editing]))
        li:not(:focus-visible) .tree-row:not(:active):hover {
      background-color: unset;
    }

    .expand-icon {
      -webkit-mask-image: url(../foreground/images/files/ui/sort_desc.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: currentColor;
      flex: none;
      height: 20px;
      padding: 6px;
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
      --xf-icon-color: var(--cros-icon-color-primary);
      flex: none;
      left: -4px;
      position: relative;
      right: -4px;
    }

    :host([selected]) .tree-label-icon {
      --xf-icon-color: var(--cros-icon-color-selection);
    }

    .tree-label {
      display: block;
      flex: auto;
      font-weight: 500;
      margin: 0 12px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: pre;
    }

    .rename {
      background-color: var(--cros-bg-color);
      border-radius: 2px;
      border: none;
      caret-color: var(--cros-textfield-cursor-color-focus);
      color: var(--cros-text-color-primary);
      margin: 0 10px;
      outline: 2px solid var(--cros-focus-ring-color);
      overflow: hidden;
    }

    paper-ripple {
      display: none;
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

  const refresh23Style = css`
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
      border-radius: 20px;
      box-sizing: border-box;
      color: var(--cros-sys-on_surface);
      cursor: pointer;
      display: flex;
      height: 40px;
      margin: 8px 0;
      position: relative;
      user-select: none;
      white-space: nowrap;
    }

    :host(:not([selected]):not([disabled]):not([editing]))
        li:not(:focus-visible) .tree-row:hover {
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

    li:focus-visible .tree-row {
      outline: 2px solid var(--cros-sys-focus_ring);
      outline-offset: 2px;
      z-index: 2;
    }

    :host-context(.pointer-active):host(:not([selected]):not([disabled]):not([editing]))
        li:not(:focus-visible) .tree-row:not(:hover):active {
      background-color: var(--cros-sys-hover_on_subtle);
    }

    :host-context(.pointer-active) .tree-row:not(:active) {
      cursor: default;
    }

    :host-context(.pointer-active):host(:not([selected]):not([disabled]):not([editing]))
        li:not(:focus-visible) .tree-row:not(:active):hover {
      background-color: unset;
    }

    .expand-icon {
      -webkit-mask-image: url(../foreground/images/files/ui/sort_desc.svg);
      -webkit-mask-position: center;
      -webkit-mask-repeat: no-repeat;
      background-color: currentColor;
      flex: none;
      height: 20px;
      margin-inline-start: 28px;
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
      font-weight: 500;
      margin-inline-start: 8px;
      overflow: hidden;
      text-overflow: ellipsis;
      white-space: pre;
    }

    .rename {
      background-color: var(--cros-sys-app_base);
      border-radius: 4px;
      border: none;
      color: var(--cros-sys-on_surface);
      height: 20px;
      margin: 0 10px;
      outline: 2px solid var(--cros-sys-focus_ring);
      overflow: hidden;
      padding: 1px 8px;
    }

    :host([selected]) .rename {
      outline: 2px solid var(--cros-sys-inverse_primary);
    }

    .rename::selection {
      background-color: var(--cros-sys-highlight_text)
    }

    paper-ripple {
      border-radius: 20px;
      color: var(--cros-sys-ripple_primary);
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

  return [
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
/** Type of the tree item collapsed custom event. */
export type TreeItemRenamedEvent = CustomEvent<{
  /** The tree item which has been renamed. */
  item: XfTreeItem,
  /** The label before rename. */
  oldLabel: string,
  /** The label after rename. */
  newLabel: string,
}>;

declare global {
  interface HTMLElementEventMap {
    [XfTreeItem.events.TREE_ITEM_EXPANDED]: TreeItemExpandedEvent;
    [XfTreeItem.events.TREE_ITEM_COLLAPSED]: TreeItemCollapsedEvent;
    [XfTreeItem.events.TREE_ITEM_RENAMED]: TreeItemRenamedEvent;
  }

  interface HTMLElementTagNameMap {
    'xf-tree-item': XfTreeItem;
  }
}
