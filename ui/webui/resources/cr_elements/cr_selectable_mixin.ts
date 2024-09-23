// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

/**
 * CrSelectableMixin maintains a collection of selectable elements. The
 * elements are queried from the light DOM, and are identified using a
 * |selectable| CSS selector, if specified.
 *
 * The mixin observes click events on its children, and selects an item when
 * clicked. Items can also be selected using the select* methods, or by
 * updating the |selected| property. The mixin sets the 'selected' CSS
 * class on the selected item, if any, and also sets the |selectedAttribute|
 * boolean attribute on the selected item if it is specified.
 *
 * Events fired:
 * iron-activate: Fired when an item is activated by a "click" event, before
 * the item is selected (see below).
 * iron-select: Fired when an item is selected.
 * iron-deselect: Fired when an item is deselected.
 * iron-items-changed: Fired when the list of selectable items changes.
 * TODO (rbpotter): Rename these events, now that they are not fired by an
 * "iron-" behavior.
 */

type Constructor<T> = new (...args: any[]) => T;

export const CrSelectableMixin = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<CrSelectableMixinInterface> => {
  class CrSelectableMixin extends superClass implements
      CrSelectableMixinInterface {
    static get properties() {
      return {
        /**
         * To use an attribute value of an element for determining `selected`
         * instead of using the index, set this property to the name of the HTML
         * attribute.
         */
        attrForSelected: {type: String},

        /**
         * Gets or sets the selected value. The default is to use the index of
         * the selected item. If attrForSelected is set, this is instead the
         * value of the |attrForSelected| attribute of the selected item.
         */
        selected: {
          type: String,
          notify: true,
        },

        /** Boolean attribute name to set on items that are selected. */
        selectedAttribute: {type: String},

        /**
         * This is a CSS selector string.  If this is set, only items that match
         * the CSS selector are selectable.
         */
        selectable: {type: String},
      };
    }

    attrForSelected: string|null = null;
    selectable?: string;
    selected?: string|number;
    selectedAttribute: string|null = null;

    // Whether to select items when they or their children are clicked. Note:
    // value is only checked in firstUpdated().
    selectOnClick: boolean = true;

    private items_: Element[] = [];
    private selectedItem_: Element|null = null;

    override firstUpdated(changedProperties: PropertyValues<this>) {
      super.firstUpdated(changedProperties);
      if (this.selectOnClick) {
        this.addEventListener('click', e => this.onClick_(e));
      }
      this.observeItems();
    }

    // Override this method in client code to modify the observation logic,
    // or to turn it off completely. By default it listens for any changes on
    // the first <slot> node in this shadowRoot.
    observeItems() {
      this.getSlot().addEventListener('slotchange', () => this.itemsChanged());
    }

    override connectedCallback() {
      super.connectedCallback();
      this.updateItems_();
    }

    override willUpdate(changedProperties: PropertyValues<this>) {
      super.willUpdate(changedProperties);

      if (changedProperties.has('attrForSelected')) {
        if (this.selectedItem_) {
          assert(this.attrForSelected);
          const value = this.selectedItem_.getAttribute(this.attrForSelected);
          assert(value !== null);
          this.selected = value;
        }
      }
    }

    override updated(changedProperties: PropertyValues<this>) {
      super.updated(changedProperties);
      if (changedProperties.has('selected')) {
        this.updateSelectedItem_();
      }
    }

    /**
     * Selects the given value.
     */
    select(value: string|number) {
      this.selected = value;
    }

    /**
     * Selects the previous item.
     */
    selectPrevious() {
      const length = this.items_.length;
      let index = length - 1;
      if (this.selected !== undefined) {
        index = ((this.valueToIndex_(this.selected)) - 1 + length) % length;
      }
      this.selected = this.indexToValue_(index);
    }

    /**
     * Selects the next item.
     */
    selectNext() {
      const index = this.selected === undefined ?
          0 :
          (this.valueToIndex_(this.selected) + 1) % this.items_.length;
      this.selected = this.indexToValue_(index);
    }

    getItemsForTest(): Element[] {
      return this.items_;
    }

    getSlot(): HTMLSlotElement {
      const slot = this.shadowRoot!.querySelector('slot');
      assert(slot);
      return slot;
    }

    // Override this method in client code to modify this logic, for example to
    // grab children that don't reside in a <slot>.
    queryItems(): Element[] {
      const selectable = this.selectable === undefined ? '*' : this.selectable;
      return Array.from(this.querySelectorAll(`:scope > ${selectable}`));
    }

    // If overriding queryItems(), override this method to return the list item
    // element matching the CSS selector string |selector|.
    queryMatchingItem(selector: string): HTMLElement|null {
      const selectable = this.selectable || '*';
      return this.querySelector<HTMLElement>(
          `:scope > :is(${selectable})${selector}`);
    }

    private updateItems_() {
      this.items_ = this.queryItems();
      this.items_.forEach(
          (item, index) =>
              item.setAttribute('data-selection-index', index.toString()));
    }

    get selectedItem(): Element|null {
      return this.selectedItem_;
    }

    private updateSelectedItem_() {
      if (!this.items_) {
        return;
      }

      const item = this.selected == null ?
          null :
          this.items_[this.valueToIndex_(this.selected)];
      if (!!item && this.selectedItem_ !== item) {
        this.setItemSelected_(this.selectedItem_, false);
        this.setItemSelected_(item, true);
      } else if (!item) {
        this.setItemSelected_(this.selectedItem_, false);
      }
    }

    private setItemSelected_(item: Element|null, isSelected: boolean) {
      if (!item) {
        return;
      }

      item.classList.toggle('selected', isSelected);
      if (this.selectedAttribute) {
        item.toggleAttribute(this.selectedAttribute, isSelected);
      }
      this.selectedItem_ = isSelected ? item : null;
      this.fire('iron-' + (isSelected ? 'select' : 'deselect'), {item: item});
    }

    private valueToIndex_(value: string|number): number {
      if (!this.attrForSelected) {
        return Number(value);
      }

      const match =
          this.queryMatchingItem(`[${this.attrForSelected}="${value}"]`);
      return match ? Number(match.dataset['selectionIndex']) : -1;
    }

    private indexToValue_(index: number): string|number {
      if (!this.attrForSelected) {
        return index;
      }

      const item = this.items_[index];
      if (!item) {
        return index;
      }

      return item.getAttribute(this.attrForSelected) || index;
    }

    itemsChanged() {
      this.updateItems_();
      this.updateSelectedItem_();

      // Let other interested parties know about the change.
      this.fire('iron-items-changed');
    }

    private onClick_(e: MouseEvent) {
      let element = e.target as HTMLElement;
      while (element && element !== this) {
        const idx = this.items_.indexOf(element);
        if (idx >= 0) {
          const value = this.indexToValue_(idx);
          assert(value !== null);
          this.fire('iron-activate', {item: element, selected: value});
          this.select(value);
          return;
        }
        element = element.parentNode as HTMLElement;
      }
    }
  }

  return CrSelectableMixin;
};

export interface CrSelectableMixinInterface {
  attrForSelected: string|null;
  selected?: string|number;
  selectable?: string;
  readonly selectedItem: Element|null;
  selectOnClick: boolean;

  getItemsForTest(): Element[];
  getSlot(): HTMLSlotElement;
  itemsChanged(): void;
  selectNext(): void;
  selectPrevious(): void;
  select(value: string|number): void;

  // Methods to override to modify default behavior.
  observeItems(): void;
  queryItems(): Element[];
  queryMatchingItem(selector: string): HTMLElement|null;
}
