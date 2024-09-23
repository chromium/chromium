// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/md_select.css.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './filter_chips.html.js';

export interface Suggestion {
  label: string;
  timeRangeStart: Date;
  ariaLabel: string;
}

function generateSuggestions(): Suggestion[] {
  const yesterday = new Date();
  yesterday.setDate(yesterday.getDate() - 1);
  yesterday.setHours(0, 0, 0, 0);
  const last7Days = new Date();
  last7Days.setDate(last7Days.getDate() - 7);
  last7Days.setHours(0, 0, 0, 0);
  const last30Days = new Date();
  last30Days.setDate(last30Days.getDate() - 30);
  last30Days.setHours(0, 0, 0, 0);

  return [
    {
      label: loadTimeData.getString('historyEmbeddingsSuggestion1'),
      timeRangeStart: yesterday,
      ariaLabel:
          loadTimeData.getString('historyEmbeddingsSuggestion1AriaLabel'),
    },
    {
      label: loadTimeData.getString('historyEmbeddingsSuggestion2'),
      timeRangeStart: last7Days,
      ariaLabel:
          loadTimeData.getString('historyEmbeddingsSuggestion2AriaLabel'),
    },
    {
      label: loadTimeData.getString('historyEmbeddingsSuggestion3'),
      timeRangeStart: last30Days,
      ariaLabel:
          loadTimeData.getString('historyEmbeddingsSuggestion3AriaLabel'),
    },
  ];
}

export interface HistoryEmbeddingsFilterChips {
  $: {
    showByGroupSelectMenu: HTMLSelectElement,
  };
}

const HistoryEmbeddingsFilterChipsElementBase = I18nMixin(PolymerElement);

export class HistoryEmbeddingsFilterChips extends
    HistoryEmbeddingsFilterChipsElementBase {
  static get is() {
    return 'cr-history-embeddings-filter-chips';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      enableShowResultsByGroupOption: Boolean,
      timeRangeStart: {
        type: Object,
        observer: 'onTimeRangeStartChanged_',
      },
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
        value: () => generateSuggestions(),
      },
    };
  }

  enableShowResultsByGroupOption: boolean;
  selectedSuggestion?: Suggestion;
  showResultsByGroup: boolean;
  private suggestions_: Suggestion[];
  timeRangeStart?: Date;

  private getByGroupIcon_(): string {
    return this.showResultsByGroup ? 'cr:check' : 'history-embeddings:by-group';
  }

  private isSuggestionSelected_(suggestion: Suggestion): boolean {
    return this.selectedSuggestion === suggestion;
  }

  private onShowByGroupSelectMenuChanged_() {
    this.showResultsByGroup = this.$.showByGroupSelectMenu.value === 'true';
  }

  private onTimeRangeStartChanged_() {
    if (this.timeRangeStart?.getTime() ===
        this.selectedSuggestion?.timeRangeStart.getTime()) {
      return;
    }

    this.selectedSuggestion = this.suggestions_.find(suggestion => {
      return suggestion.timeRangeStart.getTime() ===
          this.timeRangeStart?.getTime();
    });
  }

  private onSuggestionClick_(e: DomRepeatEvent<Suggestion>) {
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
