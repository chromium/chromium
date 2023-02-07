// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_grid.html.js';

// Displays children in a two-dimensional grid and supports focusing children
// with arrow keys.

export interface CrGridElement {
  $: {
    items: HTMLSlotElement,
  };
}

export class CrGridElement extends PolymerElement {
  static get is() {
    return 'cr-grid';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      columns: {
        type: Number,
        observer: 'onColumnsChange_',
      },
      disableArrowNavigation: Boolean,
    };
  }

  disableArrowNavigation: boolean = false;
  columns: number = 1;

  private onColumnsChange_() {
    this.updateStyles({'--cr-grid-columns': this.columns});
  }

  private onKeyDown_(e: KeyboardEvent) {
    if (!this.disableArrowNavigation &&
        ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) {
      e.preventDefault();
      const items =
          (this.$.items.assignedElements() as HTMLElement[]).filter(el => {
            return !!(
                el.offsetWidth || el.offsetHeight ||
                el.getClientRects().length);
          });
      const currentIndex = items.indexOf(e.target as HTMLElement);
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

      const newIndex = (items.length + currentIndex + delta) % items.length;
      items[newIndex]!.focus();
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
