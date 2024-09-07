// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-infinite-list' is a thin wrapper around 'cr-lazy-list' that
 * emulates some of the behavior of 'iron-list'.
 */

import '../cr_lazy_list/cr_lazy_list.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement, html, render} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues, TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_infinite_list.css.js';

export class CrInfiniteListElement<T = object> extends CrLitElement {
  static get is() {
    return 'cr-infinite-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    // Render items into light DOM using the client provided template
    render(
        html`<cr-lazy-list id="list" .scrollTarget="${this.scrollTarget}"
          .scrollOffset="${this.scrollOffset}"
          .listItemHost="${(this.getRootNode() as ShadowRoot).host}"
          .items="${this.items}"
          .template="${
            (item: T, index: number) => this.template(
                item, index, index === this.focusedIndex ? 0 : -1)}"
          .restoreFocusElement="${this.focusedItem_}"
          @keydown="${this.onKeyDown_}"
          @focusin="${this.onItemFocus_}"
          @viewport-filled="${this.updateFocusedItem_}">
        </cr-lazy-list>`,
        this, {
          host: this,
        });
    return html`<slot></slot>`;
  }

  static override get properties() {
    return {
      scrollOffset: {type: Number},
      scrollTarget: {type: Object},
      items: {type: Array},
      focusedIndex: {type: Number},
      template: {type: Object},
      focusedItem_: {type: Object},
    };
  }

  scrollOffset: number = 0;
  scrollTarget: HTMLElement = document.documentElement;
  items: T[] = [];
  // Unlike cr-lazy-list, cr-infinite-list provides a tabindex parameter for
  // clients as is provided by iron-list. Like iron-list, cr-infinite-list will
  // pass 0 for this parameter if the list item should be keyboard focusable,
  // and -1 otherwise.
  template:
      (item: T, index: number,
       tabindex: number) => TemplateResult = () => html``;
  focusedIndex: number = -1;
  private focusedItem_: HTMLElement|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('items')) {
      if (this.focusedIndex >= this.items.length) {
        this.focusedIndex = this.items.length - 1;
      } else if (this.focusedIndex === -1 && this.items.length > 0) {
        this.focusedIndex = 0;
      }
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('focusedIndex')) {
      this.updateFocusedItem_();
    }
  }

  private updateFocusedItem_() {
    this.focusedItem_ = this.focusedIndex === -1 ?
        null :
        this.querySelector<HTMLElement>(
            `cr-lazy-list > *:nth-child(${this.focusedIndex + 1})`);
  }

  private async onItemFocus_(e: Event) {
    const renderedItems =
        this.querySelectorAll<HTMLElement>('cr-lazy-list > *');
    const focusedIdx = Array.from(renderedItems).findIndex(item => {
      return item === e.target || item.shadowRoot?.activeElement === e.target;
    });

    if (focusedIdx !== -1) {
      this.focusedIndex = focusedIdx;
    }
  }

  /**
   * Handles key events when list item elements have focus.
   */
  private async onKeyDown_(e: KeyboardEvent) {
    // Do not interfere with any parent component that manages 'shift' related
    // key events.
    if (e.shiftKey || (e.key !== 'ArrowUp' && e.key !== 'ArrowDown')) {
      return;
    }
    e.stopPropagation();
    e.preventDefault();

    // Identify the new focused index.
    this.focusedIndex = e.key === 'ArrowUp' ?
        Math.max(0, this.focusedIndex - 1) :
        Math.min(this.items.length - 1, this.focusedIndex + 1);

    const list = this.querySelector('cr-lazy-list');
    assert(list);
    const element = await list.ensureItemRendered(this.focusedIndex);
    element.focus();
    element.scrollIntoViewIfNeeded();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-infinite-list': CrInfiniteListElement;
  }
}

customElements.define(CrInfiniteListElement.is, CrInfiniteListElement);
