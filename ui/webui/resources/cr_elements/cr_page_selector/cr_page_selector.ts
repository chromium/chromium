// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CrSelectableMixin} from '../cr_selectable_mixin.js';

import {getCss} from './cr_page_selector.css.js';
import {getHtml} from './cr_page_selector.html.js';

/**
 * cr-page-selector is a simple implementation of CrSelectableMixin which by
 * default hides any slotted element that is not currently marked as 'selected',
 * since this is usually leveraged to implement a page selector where only the
 * currently selected page is visible.
 *
 * A 'show-all' attribute is exposed which when set causes all slotted
 * elements (selected and non-selected) to be visible at all times, which makes
 * this element useful for more UI use cases, besides the 'page selector' case.
 */

const CrPageSelectorElementBase = CrSelectableMixin(CrLitElement);

export class CrPageSelectorElement extends CrPageSelectorElementBase {
  static get is() {
    return 'cr-page-selector';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      // Set this property to true to flatten slot items, i.e. to grab elements
      // from slots in this element's light DOM, instead of only items assigned
      // directly to this element's shadow DOM <slot>. This is done by passing
      // {flatten: true} when querying items assigned to the <slot> and by
      // observing slotchange events that bubble to this element instead of only
      // those fired on the shadow DOM <slot>. Note |selectable| is ignored
      // if hasNestedSlots = true; all assignedElements are assumed to be
      // selectable.
      hasNestedSlots: {type: Boolean},
    };
  }

  override render() {
    return getHtml.bind(this)();
  }

  hasNestedSlots: boolean = false;

  constructor() {
    super();

    // Overridden from CrSelectableMixin, since selecting pages on click does
    // not make sense (only one page is visible at a time, and this can undo
    // a selection set elsewhere).
    this.selectOnClick = false;
  }

  // CrSelectableMixin override for hasNestedSlots = true mode.
  override queryItems(): Element[] {
    return this.hasNestedSlots ?
        Array.from(this.getSlot().assignedElements({flatten: true})) :
        super.queryItems();
  }

  // CrSelectableMixin override for hasNestedSlots = true mode.
  override queryMatchingItem(selector: string): HTMLElement|null {
    if (this.hasNestedSlots) {
      const match =
          this.queryItems().find(el => (el as HTMLElement).matches(selector));
      return match ? match as HTMLElement : null;
    }
    return super.queryMatchingItem(selector);
  }

  // CrSelectableMixin override for hasNestedSlots = true mode.
  override observeItems() {
    if (this.hasNestedSlots) {
      this.addEventListener('slotchange', () => this.itemsChanged());
    }
    super.observeItems();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-page-selector': CrPageSelectorElement;
  }
}

customElements.define(CrPageSelectorElement.is, CrPageSelectorElement);
