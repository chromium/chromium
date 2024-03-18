// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';
import {hasKeyModifiers} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_grid.css.js';
import {getHtml} from './cr_grid.html.js';

// Displays children in a two-dimensional grid and supports focusing children
// with arrow keys.

export interface CrGridElement {
  $: {
    items: HTMLSlotElement,
  };
}

export class CrGridElement extends CrLitElement {
  static get is() {
    return 'cr-grid';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      columns: {type: Number},
      disableArrowNavigation: {type: Boolean},
      focusSelector: {type: String},
      ignoreModifiedKeyEvents: {type: Boolean},
    };
  }

  columns: number = 1;
  disableArrowNavigation: boolean = false;
  focusSelector?: string;
  ignoreModifiedKeyEvents: boolean = false;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('columns')) {
      this.style.setProperty('--cr-grid-columns', String(this.columns));
    }
  }

  private getSlottedParent_(element: HTMLElement): HTMLElement {
    let parent = element;

    while (parent.assignedSlot !== this.$.items &&
           parent.parentElement !== null) {
      parent = parent.parentElement;
    }

    assert(parent);
    return parent;
  }

  protected onKeyDown_(e: KeyboardEvent) {
    if (!this.disableArrowNavigation &&
        ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) {
      const items =
          (this.$.items.assignedElements() as HTMLElement[]).filter(el => {
            return !!(
                el.offsetWidth || el.offsetHeight ||
                el.getClientRects().length);
          });
      const currentIndex =
          items.indexOf(this.getSlottedParent_(e.target as HTMLElement));
      const isRtl = window.getComputedStyle(this)['direction'] === 'rtl';
      const bottomRowColumns = items.length % this.columns;
      const direction = ['ArrowRight', 'ArrowDown'].includes(e.key) ? 1 : -1;
      const inEdgeRow = direction === 1 ?
          currentIndex >= items.length - bottomRowColumns :
          currentIndex < this.columns;
      let delta = 0;
      switch (e.key) {
        case 'ArrowLeft':
        case 'ArrowRight':
          // Ignores keys likely to be browse shortcuts (like Alt+Left for
          // back).
          if (this.ignoreModifiedKeyEvents && hasKeyModifiers(e)) {
            return;
          }

          delta = direction * (isRtl ? -1 : 1);
          break;
        case 'ArrowUp':
        case 'ArrowDown':
          delta = direction * (inEdgeRow ? bottomRowColumns : this.columns);
          break;
      }
      // Handle cases where we move to an empty space in a non-full bottom row
      // and have to jump to the next row.
      if (e.key === 'ArrowUp' && inEdgeRow &&
          currentIndex >= bottomRowColumns) {
        delta -= this.columns;
      } else if (
          e.key === 'ArrowDown' && !inEdgeRow &&
          currentIndex + delta >= items.length) {
        delta += bottomRowColumns;
      }

      e.preventDefault();
      const newIndex = (items.length + currentIndex + delta) % items.length;
      const item = items[newIndex]!;
      const toFocus = this.focusSelector ?
          item.querySelector<HTMLElement>(this.focusSelector) :
          item;
      assert(toFocus);
      toFocus.focus();
    }

    if (['Enter', ' '].includes(e.key)) {
      e.preventDefault();
      e.stopPropagation();
      (e.target as HTMLElement).click();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-grid': CrGridElement;
  }
}

customElements.define(CrGridElement.is, CrGridElement);
