// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cr_autocomplete_match.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_autocomplete_match_list.html.js';
import {AutocompleteResult, PageCallbackRouter, PageHandler, PageHandlerInterface} from './omnibox.mojom-webui.js';

// Displays the autocomplete matches in the autocomplete result.
export class AutocompleteMatchListElement extends PolymerElement {
  static get is() {
    return 'cr-autocomplete-match-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      result_: Object,
    };
  }

  private callbackRouter_: PageCallbackRouter;
  private omniboxAutocompleteResultChangedListenerId_: number|null = null;
  private pageHandler_: PageHandlerInterface;
  private result_: AutocompleteResult;

  constructor() {
    super();
    this.pageHandler_ = PageHandler.getRemote();
    this.callbackRouter_ = new PageCallbackRouter();
    this.pageHandler_.setPage(
        this.callbackRouter_.$.bindNewPipeAndPassRemote());
  }

  override connectedCallback() {
    super.connectedCallback();
    this.omniboxAutocompleteResultChangedListenerId_ =
        this.callbackRouter_.omniboxAutocompleteResultChanged.addListener(
            this.onOmniboxAutocompleteResultChanged_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.omniboxAutocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.omniboxAutocompleteResultChangedListenerId_);
  }

  private onOmniboxAutocompleteResultChanged_(result: AutocompleteResult) {
    this.result_ = result;
  }

  private onResultRepaint_() {
    const metricsReporter = MetricsReporterImpl.getInstance();
    metricsReporter.measure('ResultChanged')
        .then(
            duration => metricsReporter.umaReportTime(
                'WebUIOmnibox.ResultChangedToRepaintLatency.ToPaint', duration))
        .then(() => metricsReporter.clearMark('ResultChanged'))
        // Ignore silently if mark 'ResultChanged' is missing.
        .catch(() => {});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-autocomplete-match-list': AutocompleteMatchListElement;
  }
}

customElements.define(
    AutocompleteMatchListElement.is, AutocompleteMatchListElement);
