// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-infinite-list' is a thin wrapper around 'cr-lazy-list' that
 * emulates some of the behavior of 'iron-list'.
 * In particular, 'cr-infinite-list':
 *   - Tracks a `focusedIndex`, and uses this to pass a `tabindex` to the
 *     template method.
 *   - If list items change while focus is within the element at
 *     `focusedIndex`, 'cr-infinite-list' will restore focus once rendering is
 *     complete. Before restoring focus, a restore-list-focus event is fired.
 *     Clients using FocusRowMixinLit should reset listBlurred = false when
 *     this event is fired so the mixin correctly handles the following focus()
 *     call.
 *   - Implements the arrow key up/down navigation behavior that was provided
 *     by 'iron-list'.
 */

import '../cr_lazy_list/cr_lazy_list.js';

import {assert} from '//resources/js/assert.js';
import type {PropertyValues, TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement, html, render} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_infinite_list.css.js';

export class CrInfiniteListElement<T> extends CrLitElement {
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
          .chunkSize="${this.chunkSize}"
          .scrollOffset="${this.scrollOffset}"
          .listItemHost="${(this.getRootNode() as ShadowRoot).host}"
          .items="${this.items}" .itemSize="${this.itemSize}"
          .template="${
            (item: T, index: number) => this.template(
                item, index, index === this.focusedIndex ? 0 : -1)}"
          @keydown="${this.onKeyDown_}"
          @focusin="${this.onFocusIn_}"
          @viewport-filled="${this.onViewportFilled_}">
        </cr-lazy-list>`,
        this, {
          host: this,
        });
    return html`<slot></slot>`;
  }

  static override get properties() {
    return {
      chunkSize: {type: Number},
      scrollOffset: {type: Number},
      scrollTarget: {type: Object},
      usingDefaultScrollTarget: {
        type: Boolean,
        reflect: true,
      },
      items: {type: Array},
      focusedIndex: {type: Number},
      itemSize: {type: Number},
      template: {type: Object},
    };
  }

  accessor chunkSize: number = 0;
  accessor scrollOffset: number = 0;
  accessor scrollTarget: HTMLElement = this;
  accessor usingDefaultScrollTarget: boolean = true;
  accessor items: T[] = [];
  accessor itemSize: number|undefined = undefined;
  // Unlike cr-lazy-list, cr-infinite-list provides a tabindex parameter for
  // clients as is provided by iron-list. Like iron-list, cr-infinite-list will
  // pass 0 for this parameter if the list item should be keyboard focusable,
  // and -1 otherwise.
  accessor template:
      (item: T, index: number,
       tabindex: number) => TemplateResult = () => html``;
  accessor focusedIndex: number = -1;
  private restoreFocus_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('scrollTarget')) {
      this.usingDefaultScrollTarget = this.scrollTarget === this;
    }

    if (changedProperties.has('items')) {
      if (this.isFocusWithinList_()) {
        this.restoreFocus_ = true;
      }
      if (this.focusedIndex >= this.items.length) {
        this.focusedIndex = this.items.length - 1;
      } else if (this.focusedIndex === -1 && this.items.length > 0) {
        this.focusedIndex = 0;
      }
    }
  }

  private isFocusWithinList_(): boolean {
    if (this.focusedIndex === -1) {
      return false;
    }

    const list = this.querySelector('cr-lazy-list');
    assert(list);

    const renderedItems = list.domItems();
    return renderedItems[this.focusedIndex]!.matches(':focus-within');
  }

  fillCurrentViewport(): Promise<void> {
    const list = this.querySelector('cr-lazy-list');
    assert(list);
    return list.fillCurrentViewport();
  }

  ensureItemRendered(index: number): Promise<HTMLElement> {
    const list = this.querySelector('cr-lazy-list');
    assert(list);
    return list.ensureItemRendered(index);
  }

  private onViewportFilled_() {
    if (!this.restoreFocus_) {
      return;
    }
    this.restoreFocus_ = false;

    // Wait 1 macrotask for clients to catch the event and propagate the
    // state via data bindings before focusing the relevant item.
    this.fire('restore-list-focus');
    setTimeout(async () => {
      if (this.focusedIndex >= 0 && this.focusedIndex < this.items.length) {
        const item = await this.ensureItemRendered(this.focusedIndex);
        item.focus();
      }
    }, 0);
  }

  private onFocusIn_(e: Event) {
    const list = this.querySelector('cr-lazy-list');
    assert(list);
    const renderedItems = list.domItems();
    const focusedIdx = Array.from(renderedItems).findIndex(item => {
      return item === e.target || item.contains(e.target as Node) ||
          item.shadowRoot?.activeElement === e.target;
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
    const newIndex = e.key === 'ArrowUp' ?
        Math.max(0, this.focusedIndex - 1) :
        Math.min(this.items.length - 1, this.focusedIndex + 1);
    if (newIndex === this.focusedIndex) {
      return;
    }

    // Await updateComplete to allow the focusedIndex change to properly
    // propagate to the DOM before focusing the relevant item.
    this.focusedIndex = newIndex;
    await this.updateComplete;

    const list = this.querySelector('cr-lazy-list');
    assert(list);
    const element = await list.ensureItemRendered(this.focusedIndex);
    element.focus();
    element.scrollIntoViewIfNeeded();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-infinite-list': CrInfiniteListElement<unknown>;
  }
}

customElements.define(CrInfiniteListElement.is, CrInfiniteListElement);
