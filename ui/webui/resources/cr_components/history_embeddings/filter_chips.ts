// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.html.js';

import type {IronIconElement} from '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './filter_chips.html.js';

export interface HistoryEmbeddingsFilterChips {
  $: {
    byGroupChip: HTMLElement,
    byGroupChipIcon: IronIconElement,
  };
}
export class HistoryEmbeddingsFilterChips extends PolymerElement {
  static get is() {
    return 'cr-history-embeddings-filter-chips';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showResultsByGroup: {
        type: Boolean,
        notify: true,
        value: false,
      },
    };
  }

  showResultsByGroup: boolean;

  private getByGroupIcon_(): string {
    return this.showResultsByGroup ? 'cr:check' : 'history-embeddings:by-group';
  }

  private onByGroupClick_() {
    this.showResultsByGroup = !this.showResultsByGroup;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-history-embeddings-filter-chips': HistoryEmbeddingsFilterChips;
  }
}

customElements.define(
    HistoryEmbeddingsFilterChips.is, HistoryEmbeddingsFilterChips);
