// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_chip/cr_chip.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './filter_chips.css.js';
import {getHtml} from './filter_chips.html.js';

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

const HistoryEmbeddingsFilterChipsElementBase = I18nMixinLit(CrLitElement);

export class HistoryEmbeddingsFilterChips extends
    HistoryEmbeddingsFilterChipsElementBase {
  static get is() {
    return 'cr-history-embeddings-filter-chips';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enableShowResultsByGroupOption: {
        type: Boolean,
      },
      timeRangeStart: {
        type: Object,
      },
      selectedSuggestion: {
        type: String,
        notify: true,
      },
      showResultsByGroup: {
        type: Boolean,
        notify: true,
      },
      suggestions_: {
        type: Array,
      },
    };
  }

  accessor enableShowResultsByGroupOption: boolean = false;
  accessor selectedSuggestion: Suggestion|undefined;
  accessor showResultsByGroup: boolean = false;
  protected accessor suggestions_: Suggestion[] = generateSuggestions();
  accessor timeRangeStart: Date|undefined;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('timeRangeStart')) {
      this.onTimeRangeStartChanged_();
    }
  }

  protected isSuggestionSelected_(suggestion: Suggestion): boolean {
    return this.selectedSuggestion === suggestion;
  }

  protected onShowByGroupSelectMenuChanged_() {
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

  protected onSuggestionClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const clickedSuggestion = this.suggestions_[index];
    assert(clickedSuggestion);
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

export type FilterChipsElement = HistoryEmbeddingsFilterChips;

customElements.define(
    HistoryEmbeddingsFilterChips.is, HistoryEmbeddingsFilterChips);
