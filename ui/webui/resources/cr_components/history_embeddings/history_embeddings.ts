// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import './icons.html.js';
import './result_image.js';

import {HistoryResultType, QUERY_RESULT_MINIMUM_AGE} from '//resources/cr_components/history/constants.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {CrLazyRenderElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HistoryEmbeddingsBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './history_embeddings.html.js';
import type {SearchQuery, SearchResult, SearchResultItem} from './history_embeddings.mojom-webui.js';
import {AnswerStatus, UserFeedback} from './history_embeddings.mojom-webui.js';

function jsDateToMojoDate(date: Date): Time {
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const internalValue = BigInt(date.valueOf() + epochDeltaInMs) * BigInt(1000);
  return {internalValue};
}

/* Minimum time the loading state should be visible. This is to prevent the
 * loading animation from flashing. */
export const LOADING_STATE_MINIMUM_MS = 300;

export interface HistoryEmbeddingsElement {
  $: {
    sharedMenu: CrLazyRenderElement<CrActionMenuElement>,
  };
}

export type HistoryEmbeddingsMoreActionsClickEvent =
    CustomEvent<SearchResultItem>;

declare global {
  interface HTMLElementEventMap {
    'more-from-site-click': HistoryEmbeddingsMoreActionsClickEvent;
    'remove-item-click': HistoryEmbeddingsMoreActionsClickEvent;
  }
}

const HistoryEmbeddingsElementBase = I18nMixin(PolymerElement);

export class HistoryEmbeddingsElement extends HistoryEmbeddingsElementBase {
  static get is() {
    return 'cr-history-embeddings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      clickedIndices_: Array,
      forceSuppressLogging: Boolean,
      numCharsForQuery: Number,
      feedbackState_: {
        type: String,
        value: CrFeedbackOption.UNSPECIFIED,
      },
      loadingAnswer_: Boolean,
      loadingResults_: Boolean,
      searchResult_: Object,
      searchQuery: String,
      timeRangeStart: Object,
      isEmpty: {
        type: Boolean,
        reflectToAttribute: true,
        value: true,
        computed:
            'computeIsEmpty_(loadingResults_, searchResult_.items.length)',
        notify: true,
      },
      enableAnswers_: {
        type: Boolean,
        reflectToAttribute: true,
        value: () => loadTimeData.getBoolean('enableHistoryEmbeddingsAnswers'),
      },
      enableImages_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('enableHistoryEmbeddingsImages'),
      },
      answerSource_: {
        type: Object,
        computed: 'computeAnswerSource_(loadingAnswer_, searchResult_.items)',
        value: null,
      },
      showMoreFromSiteMenuOption: {
        type: Boolean,
        value: false,
      },
      showRelativeTimes: {type: Boolean, value: false},
    };
  }

  static get observers() {
    return [
      'onSearchQueryChanged_(searchQuery, timeRangeStart)',
    ];
  }

  private actionMenuItem_: SearchResultItem|null = null;
  private answerSource_: SearchResultItem|null = null;
  private browserProxy_ = HistoryEmbeddingsBrowserProxyImpl.getInstance();
  private clickedIndices_: Set<number> = new Set();
  private enableAnswers_: boolean;
  private feedbackState_: CrFeedbackOption;
  private loadingAnswer_ = false;
  private loadingResults_ = false;
  private loadingStateMinimumMs_ = LOADING_STATE_MINIMUM_MS;
  private queryResultMinAge_ = QUERY_RESULT_MINIMUM_AGE;
  private searchResult_: SearchResult|null = null;
  private searchTimestamp_: number = 0;
  /**
   * When this is non-null, that means there's a SearchResult that's pending
   * metrics logging since this debouncer timestamp. The debouncing is needed
   * because queries are issued as the user types, and we want to skip logging
   * these trivial queries the user typed through.
   */
  private resultPendingMetricsTimestamp_: number|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  forceSuppressLogging: boolean;
  isEmpty: boolean;
  numCharsForQuery: number = 0;
  private numCharsForLastResultQuery_: number = 0;
  searchQuery: string;
  timeRangeStart?: Date;
  private searchResultChangedId_: number|null = null;
  /**
   * A promise of a setTimeout for the first set of search results to come back
   * from a search. The loading state has a minimum time it needs to be on the
   * screen before showing the first set of search results, and any subsequent
   * search result for the same query is queued after it.
   */
  private searchResultPromise_: Promise<void>|null = null;
  showRelativeTimes: boolean = false;
  showMoreFromSiteMenuOption: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'beforeunload', () => {
      // Flush any metrics or logs when the user leaves the page, such as
      // closing the tab or navigating to another URL.
      this.flushDebouncedUserMetrics_(/* forceFlush= */ true);
    });
    this.searchResultChangedId_ =
        this.browserProxy_.callbackRouter.searchResultChanged.addListener(
            this.searchResultChanged_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    // Flush any metrics or logs when the component is removed, which can
    // happen if there are no history embedding results left or if the user
    // navigated to another history page.
    this.flushDebouncedUserMetrics_(/* forceFlush= */ true);
    this.eventTracker_.removeAll();
    if (this.searchResultChangedId_ !== null) {
      this.browserProxy_.callbackRouter.removeListener(
          this.searchResultChangedId_);
      this.searchResultChangedId_ = null;
    }
  }

  private computeAnswerSource_(): SearchResultItem|null {
    if (!this.enableAnswers_ || this.loadingAnswer_) {
      return null;
    }
    return this.searchResult_?.items.find(item => item.answerData) || null;
  }

  private computeIsEmpty_(): boolean {
    return !this.loadingResults_ && this.searchResult_?.items.length === 0;
  }

  private getAnswerOrError_(): string|undefined {
    if (!this.searchResult_) {
      return undefined;
    }

    // TODO(b/348689167): Move strings into a grdp file.
    switch (this.searchResult_.answerStatus) {
      case AnswerStatus.kUnspecified:
      case AnswerStatus.kLoading:
        // Still loading or in an undefined state.
        return undefined;
      case AnswerStatus.kSuccess:
        return this.searchResult_.answer;
      case AnswerStatus.kUnanswerable:
        return 'Sorry, can\'t help you with that.';
      case AnswerStatus.kModelUnavailable:
      case AnswerStatus.kExecutionFailure:
      case AnswerStatus.kExecutionCanceled:
        return 'Something went wrong. Please try again later.';
      default:
        assertNotReached();
    }
  }

  private getAnswerSourceUrl_(): string|undefined {
    if (!this.answerSource_) {
      return undefined;
    }
    const sourceUrl = new URL(this.answerSource_.url.url);
    const textDirectives = this.answerSource_.answerData?.answerTextDirectives;
    if (textDirectives && textDirectives.length > 0) {
      // Only the first directive is used for now until there's a way to show
      // multiple links in the UI.
      sourceUrl.hash = `:~:text=${encodeURIComponent(textDirectives[0])}`;
    }
    return sourceUrl.toString();
  }

  private getFavicon_(item: SearchResultItem|undefined): string {
    return getFaviconForPageURL(
        item?.url.url || '', /*isSyncedUrlForHistoryUi=*/ true);
  }

  private getHeadingText_(): string {
    if (this.loadingResults_) {
      return this.i18n('historyEmbeddingsHeadingLoading', this.searchQuery);
    }
    return this.i18n('historyEmbeddingsHeading', this.searchQuery);
  }

  private getDateTime_(item: SearchResultItem|undefined): string {
    if (!item) {
      return '';
    }

    if (this.showRelativeTimes) {
      return item.relativeTime;
    }
    return item.shortDateTime;
  }

  private hasAnswer_(): boolean {
    if (!this.enableAnswers_) {
      return false;
    }
    return this.searchResult_?.answer !== '';
  }

  private onFeedbackSelectedOptionChanged_(
      e: CustomEvent<{value: CrFeedbackOption}>) {
    this.feedbackState_ = e.detail.value;
    switch (e.detail.value) {
      case CrFeedbackOption.UNSPECIFIED:
        this.browserProxy_.setUserFeedback(
            UserFeedback.kUserFeedbackUnspecified);
        return;
      case CrFeedbackOption.THUMBS_UP:
        this.browserProxy_.setUserFeedback(UserFeedback.kUserFeedbackPositive);
        return;
      case CrFeedbackOption.THUMBS_DOWN:
        this.browserProxy_.setUserFeedback(UserFeedback.kUserFeedbackNegative);
        return;
    }
  }

  private onMoreActionsClick_(e: DomRepeatEvent<SearchResultItem>) {
    e.preventDefault();
    e.stopPropagation();

    const target = e.target as HTMLElement;
    const item = e.model.item;
    this.actionMenuItem_ = item;
    this.$.sharedMenu.get().showAt(target);
  }

  private onMoreFromSiteClick_() {
    assert(this.actionMenuItem_);
    this.dispatchEvent(new CustomEvent(
        'more-from-site-click',
        {detail: this.actionMenuItem_, bubbles: true, composed: true}));
    this.$.sharedMenu.get().close();
  }

  private onRemoveFromHistoryClick_() {
    assert(this.searchResult_);
    assert(this.actionMenuItem_);
    this.splice(
        'searchResult_.items',
        this.searchResult_.items.indexOf(this.actionMenuItem_), 1);
    this.dispatchEvent(new CustomEvent(
        'remove-item-click',
        {detail: this.actionMenuItem_, bubbles: true, composed: true}));
    this.$.sharedMenu.get().close();
  }

  private onResultClick_(e: DomRepeatEvent<SearchResultItem>) {
    this.dispatchEvent(new CustomEvent('result-click', {detail: e.model.item}));

    this.dispatchEvent(new CustomEvent('record-history-link-click', {
      bubbles: true,
      composed: true,
      detail: {
        resultType: HistoryResultType.EMBEDDINGS,
        index: e.model.index,
      },
    }));

    this.clickedIndices_.add(e.model.index);
    this.browserProxy_.recordSearchResultsMetrics(true, true);
  }

  private onSearchQueryChanged_() {
    // Flush any old results metrics before overwriting the member variable.
    this.flushDebouncedUserMetrics_();
    this.clickedIndices_.clear();

    // Cache the amount of characters that the user typed for this query so
    // that it can be sent with the quality log since `numCharsForQuery` will
    // immediately change when a new query is performed.
    this.numCharsForLastResultQuery_ = this.numCharsForQuery;

    this.searchResultPromise_ = null;
    this.loadingResults_ = true;
    this.loadingAnswer_ = false;

    const query: SearchQuery = {
      query: this.searchQuery,
      timeRangeStart:
          this.timeRangeStart ? jsDateToMojoDate(this.timeRangeStart) : null,
    };
    this.searchTimestamp_ = performance.now();
    this.browserProxy_.search(query);
  }

  private searchResultChanged_(result: SearchResult) {
    if (this.searchResultPromise_) {
      // If there is already a search result waiting to be processed, chain
      // this result to it so that it immediately runs after the previous
      // result was processed.
      this.searchResultPromise_ = this.searchResultPromise_.then(
          () => this.searchResultChangedImpl_(result));
    } else {
      // Artificial delay of loadingStateMinimumMs_ for UX.
      this.searchResultPromise_ = new Promise((resolve) => {
        setTimeout(
            () => {
              this.searchResultChangedImpl_(result);
              resolve();
            },
            Math.max(
                0,
                this.searchTimestamp_ + this.loadingStateMinimumMs_ -
                    performance.now()));
      });
    }
  }

  private searchResultChangedImpl_(result: SearchResult) {
    if (result.query !== this.searchQuery) {
      // Results are for an outdated query. Skip these results.
      return;
    }

    // Reset feedback state for new results.
    this.feedbackState_ = CrFeedbackOption.UNSPECIFIED;
    this.searchResult_ = result;
    this.loadingResults_ = false;
    this.loadingAnswer_ = result.answerStatus === AnswerStatus.kLoading;

    this.resultPendingMetricsTimestamp_ = performance.now();
  }

  private showAnswerSection_(): boolean {
    if (!this.searchResult_) {
      // If there is no search result yet, the search has just started and it
      // is not yet known if an answer is being attempted.
      return false;
    } else if (this.searchResult_.query !== this.searchQuery) {
      // The current search result and its answer is outdated.
      return false;
    } else {
      // Intent has not been computed yet. Anything related to the answer should
      // only be shown when intent computation has at least determined the query
      // is answerable.
      return this.searchResult_.answerStatus !== AnswerStatus.kUnspecified;
    }
  }

  /**
   * Flushes any pending query result metric or log waiting to be logged.
   */
  private flushDebouncedUserMetrics_(forceFlush = false) {
    if (this.resultPendingMetricsTimestamp_ === null) {
      return;
    }
    const userClickedResult = this.clickedIndices_.size > 0;
    // Search results are fetched as the user is typing, so make sure that
    // the last set of results were visible on the page for at least 2s. This is
    // to avoid logging results that may have been transient as the user was
    // still typing their full query.
    const resultsWereStable =
        (performance.now() - this.resultPendingMetricsTimestamp_) >=
        this.queryResultMinAge_;
    const canLog = resultsWereStable || forceFlush;

    // Record a metric if a user did not click any results.
    if (canLog && !userClickedResult) {
      const nonEmptyResults: boolean = !!this.searchResult_ &&
          this.searchResult_.items && this.searchResult_.items.length > 0;
      this.browserProxy_.recordSearchResultsMetrics(nonEmptyResults, false);
    }

    if (!this.forceSuppressLogging && canLog) {
      this.browserProxy_.sendQualityLog(
          Array.from(this.clickedIndices_), this.numCharsForLastResultQuery_);
    }

    // Clear this regardless if it was recorded or not, because we don't want
    // to "try again" to record the same query.
    this.resultPendingMetricsTimestamp_ = null;
  }

  overrideLoadingStateMinimumMsForTesting(ms: number) {
    this.loadingStateMinimumMs_ = ms;
  }

  overrideQueryResultMinAgeForTesting(ms: number) {
    this.queryResultMinAge_ = ms;
  }

  searchResultChangedForTesting(result: SearchResult) {
    this.searchResultChanged_(result);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-history-embeddings': HistoryEmbeddingsElement;
  }
}

customElements.define(HistoryEmbeddingsElement.is, HistoryEmbeddingsElement);
