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
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

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
      selectedSuggestion: {
        type: String,
        notify: true,
      },
      showResultsByGroup: {
        type: Boolean,
        notify: true,
        value: false,
      },
      suggestions_: {
        type: Array,
        value: () => {
          return [
            loadTimeData.getString('historyEmbeddingsSuggestion1')
                .toLowerCase(),
            loadTimeData.getString('historyEmbeddingsSuggestion2')
                .toLowerCase(),
            loadTimeData.getString('historyEmbeddingsSuggestion3')
                .toLowerCase(),
          ];
        },
      },
    };
  }

  selectedSuggestion?: string;
  showResultsByGroup: boolean;
  private suggestions_: string[];

  private getByGroupIcon_(): string {
    return this.showResultsByGroup ? 'cr:check' : 'history-embeddings:by-group';
  }

  private isSuggestionSelected_(suggestion: string): boolean {
    return this.selectedSuggestion === suggestion;
  }

  private onByGroupClick_() {
    this.showResultsByGroup = !this.showResultsByGroup;
  }

  private onSuggestionClick_(e: DomRepeatEvent<string>) {
    const clickedSuggestion = e.model.item;
    if (this.isSuggestionSelected_(clickedSuggestion)) {
      this.selectedSuggestion = undefined;
    } else {
      this.selectedSuggestion = clickedSuggestion;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-history-embeddings-filter-chips': HistoryEmbeddingsFilterChips;
  }
}

customElements.define(
    HistoryEmbeddingsFilterChips.is, HistoryEmbeddingsFilterChips);
