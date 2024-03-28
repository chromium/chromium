// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import type {CrLitElement, PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

function toCamelCase(name: string): string {
  const pieces = name.split('-');
  let camel = pieces[0];
  pieces.slice(1).forEach(
      piece => camel = camel + piece[0].toUpperCase() + piece.substr(1));
  return camel;
}

/**
 * CrSelectableMixin maintains a collection of selectable elements. The
 * elements are queried from a <slot>'s assignedElements, and are identified
 * using a |selectable| CSS selector, if specified. dom-if, dom-repeat,
 * dom-bind, and template children are excluded regardless of the value of
 * |selectable|.
 *
 * The mixin observes click events on its children, and selects an item when
 * clicked. Items can also be selected using the select* methods, or by
 * updating the |selected| property. The mixin sets the 'iron-selected' CSS
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
         * To use an attribute value or property of an element for
         * `selected` instead of the index, set this to the name of the
         * attribute or property. Hyphenated values are converted to camel case
         * when used to look up the property of a selectable element. Camel
         * cased values are *not* converted to hyphenated values for attribute
         * lookup. It's recommended that you provide the hyphenated form of the
         * name so that selection works in both cases. (Use
         * `attr-or-property-name` instead of `attrOrPropertyName`.)
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
    private items_: Element[] = [];
    private observer_: MutationObserver;
    private selectedItem_: Element|null = null;

    override firstUpdated(changedProperties: PropertyValues<this>) {
      super.firstUpdated(changedProperties);
      this.addEventListener('click', e => this.onClick_(e));
      this.getSlot_().addEventListener(
          'slotchange', () => this.itemsChanged_());
    }

    override connectedCallback() {
      super.connectedCallback();
      this.updateItems_();
    }

    override willUpdate(changedProperties: PropertyValues<this>) {
      super.willUpdate(changedProperties);

      if (changedProperties.has('attrForSelected')) {
        if (this.selectedItem_) {
          const value = this.valueForItem_(this.selectedItem_);
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

    private getSlot_(): HTMLSlotElement {
      const slot = this.shadowRoot!.querySelector('slot');
      assert(slot);
      return slot;
    }

    private updateItems_() {
      const elements = this.getSlot_().assignedElements();
      const excluded = ['template', 'dom-bind', 'dom-if', 'dom-repeat'];
      this.items_ = elements.filter(el => {
        if (excluded.includes(el.tagName)) {
          return false;
        }
        return !this.selectable || el.matches(this.selectable);
      });
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

      item.classList.toggle('iron-selected', isSelected);
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

      return this.items_.findIndex(item => this.valueForItem_(item) === value);
    }

    private indexToValue_(index: number): string|number {
      if (!this.attrForSelected) {
        return index;
      }

      const item = this.items_[index];
      if (!item) {
        return index;
      }

      const value = this.valueForItem_(item);
      return value === null ? index : value;
    }

    private valueForItem_(item: Element|null): string|number|null {
      if (!item || (!this.attrForSelected && !this.items_)) {
        return null;
      }
      if (!this.attrForSelected) {
        const index = this.items_.indexOf(item);
        return index === -1 ? null : index;
      }
      const itemAsDict = item as (Element & {[key: string]: any});
      const propValue = itemAsDict[toCamelCase(this.attrForSelected)];
      return (propValue !== undefined && propValue !== null) ?
          propValue :
          item.getAttribute(this.attrForSelected);
    }

    private itemsChanged_() {
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

  getItemsForTest(): Element[];
  select(value: string|number): void;
  selectNext(): void;
  selectPrevious(): void;
}
