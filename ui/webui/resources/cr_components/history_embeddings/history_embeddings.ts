// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';
import './icons.html.js';
import './result_image.js';

import {HistoryResultType, QUERY_RESULT_MINIMUM_AGE} from '//resources/cr_components/history/constants.js';
import {getInstance as getAnnouncerInstance} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrFeedbackOption} from '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import type {CrLazyRenderLitElement} from '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {HistoryEmbeddingsBrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './history_embeddings.css.js';
import {getHtml} from './history_embeddings.html.js';
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
    sharedMenu: CrLazyRenderLitElement<CrActionMenuElement>,
  };
}

export type HistoryEmbeddingsResultClickEvent = CustomEvent<{
  item: SearchResultItem,
  middleButton: boolean,
  altKey: boolean,
  ctrlKey: boolean,
  metaKey: boolean,
  shiftKey: boolean,
}>;

export type HistoryEmbeddingsResultContextMenuEvent = CustomEvent<{
  item: SearchResultItem,
  x: number,
  y: number,
}>;

export type HistoryEmbeddingsMoreActionsClickEvent =
    CustomEvent<SearchResultItem>;

declare global {
  interface HTMLElementEventMap {
    'more-from-site-click': HistoryEmbeddingsMoreActionsClickEvent;
    'remove-item-click': HistoryEmbeddingsMoreActionsClickEvent;
  }
}

const HistoryEmbeddingsElementBase = I18nMixinLit(CrLitElement);

export class HistoryEmbeddingsElement extends HistoryEmbeddingsElementBase {
  static get is() {
    return 'cr-history-embeddings';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      clickedIndices_: {type: Array},
      forceSuppressLogging: {type: Boolean},
      numCharsForQuery: {type: Number},
      feedbackState_: {type: String},
      loadingAnswer_: {type: Boolean},
      loadingResults_: {type: Boolean},
      searchResult_: {type: Object},
      searchResultDirty_: {type: Boolean},
      searchQuery: {type: String},
      timeRangeStart: {type: Object},

      isEmpty: {
        type: Boolean,
        reflect: true,
        notify: true,
      },

      enableAnswers_: {
        type: Boolean,
        reflect: true,
      },

      enableImages_: {type: Boolean},
      answerSource_: {type: Object},
      showMoreFromSiteMenuOption: {type: Boolean},
      showRelativeTimes: {type: Boolean},
      otherHistoryResultClicked: {type: Boolean},

      inSidePanel: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  private actionMenuItem_: SearchResultItem|null = null;
  protected accessor answerSource_: SearchResultItem|null = null;
  private answerLinkClicked_: boolean = false;
  private browserProxy_ = HistoryEmbeddingsBrowserProxyImpl.getInstance();
  private accessor clickedIndices_: Set<number> = new Set();
  protected accessor enableAnswers_: boolean =
      loadTimeData.getBoolean('enableHistoryEmbeddingsAnswers');
  protected accessor enableImages_: boolean =
      loadTimeData.getBoolean('enableHistoryEmbeddingsImages');
  protected accessor feedbackState_: CrFeedbackOption =
      CrFeedbackOption.UNSPECIFIED;
  protected accessor loadingAnswer_ = false;
  protected accessor loadingResults_ = false;
  private loadingStateMinimumMs_ = LOADING_STATE_MINIMUM_MS;
  private queryResultMinAge_ = QUERY_RESULT_MINIMUM_AGE;
  protected accessor searchResult_: SearchResult|null = null;
  protected accessor searchResultDirty_: boolean = false;
  private searchTimestamp_: number = 0;
  /**
   * When this is non-null, that means there's a SearchResult that's pending
   * metrics logging since this debouncer timestamp. The debouncing is needed
   * because queries are issued as the user types, and we want to skip logging
   * these trivial queries the user typed through.
   */
  private resultPendingMetricsTimestamp_: number|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  accessor forceSuppressLogging: boolean = false;
  accessor isEmpty: boolean = true;
  accessor numCharsForQuery: number = 0;
  private numCharsForLastResultQuery_: number = 0;
  accessor searchQuery: string = '';
  accessor timeRangeStart: Date|undefined;
  private searchResultChangedId_: number|null = null;
  /**
   * A promise of a setTimeout for the first set of search results to come back
   * from a search. The loading state has a minimum time it needs to be on the
   * screen before showing the first set of search results, and any subsequent
   * search result for the same query is queued after it.
   */
  private searchResultPromise_: Promise<void>|null = null;
  accessor showRelativeTimes: boolean = false;
  accessor showMoreFromSiteMenuOption: boolean = false;
  accessor otherHistoryResultClicked: boolean = false;
  accessor inSidePanel: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(window, 'beforeunload', () => {
      // Flush any metrics or logs when the user leaves the page, such as
      // closing the tab or navigating to another URL.
      this.flushDebouncedUserMetrics_(/* forceFlush= */ true);
    });
    if (this.inSidePanel) {
      // Side panel UI does not fire 'beforeunload' events. Instead, the
      // visibilityState is changed when the side panel is closed.
      this.eventTracker_.add(document, 'visibilitychange', () => {
        if (document.visibilityState === 'hidden') {
          this.flushDebouncedUserMetrics_();
        }
      });
    }
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('loadingResults_') ||
        changedPrivateProperties.has('searchResult_') ||
        (changedPrivateProperties.has('searchResultDirty_') &&
         this.searchResultDirty_)) {
      this.isEmpty = this.computeIsEmpty_();
    }

    if (changedPrivateProperties.has('loadingAnswer_') ||
        changedPrivateProperties.has('searchResult_') ||
        (changedPrivateProperties.has('searchResultDirty_') &&
         this.searchResultDirty_)) {
      this.answerSource_ = this.computeAnswerSource_();
    }

    const isSearchQueryInitialization =
        changedProperties.get('searchQuery') === undefined &&
        this.searchQuery === '';
    if ((changedProperties.has('searchQuery') &&
         !isSearchQueryInitialization) ||
        changedProperties.has('timeRangeStart')) {
      this.onSearchQueryChanged_();
    }

    if (changedPrivateProperties.has('searchResultDirty_') &&
        this.searchResultDirty_) {
      this.searchResultDirty_ = false;
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

  protected getAnswerOrError_(): string|undefined {
    if (!this.searchResult_) {
      return undefined;
    }

    switch (this.searchResult_.answerStatus) {
      case AnswerStatus.kUnspecified:
      case AnswerStatus.kLoading:
      case AnswerStatus.kExecutionCanceled:
      case AnswerStatus.kUnanswerable:
      case AnswerStatus.kFiltered:
      case AnswerStatus.kModelUnavailable:
        // Still loading or answer section is not displayed.
        return undefined;
      case AnswerStatus.kSuccess:
        return this.searchResult_.answer;
      case AnswerStatus.kExecutionFailure:
        return this.i18n('historyEmbeddingsAnswererErrorTryAgain');
      default:
        assertNotReached();
    }
  }

  protected getAnswerSourceUrl_(): string|undefined {
    if (!this.answerSource_) {
      return undefined;
    }
    const sourceUrl = new URL(this.answerSource_.url.url);
    const textDirectives = this.answerSource_.answerData?.answerTextDirectives;
    if (textDirectives && textDirectives.length > 0) {
      // Only the first directive is used for now until there's a way to show
      // multiple links in the UI. If the directive contains a comma, it is
      // intended to be part of the start,end syntax and should not be encoded.
      sourceUrl.hash = `:~:text=${
          textDirectives[0]!.split(',').map(encodeURIComponent).join(',')}`;
    }
    return sourceUrl.toString();
  }

  protected getFavicon_(item: SearchResultItem|undefined): string {
    return getFaviconForPageURL(
        item?.url.url || '', /*isSyncedUrlForHistoryUi=*/ true);
  }

  protected getHeadingText_(): string {
    if (this.loadingResults_) {
      return this.i18n('historyEmbeddingsHeadingLoading', this.searchQuery);
    }

    if (this.enableAnswers_) {
      return this.i18n('historyEmbeddingsWithAnswersResultsHeading');
    }

    return this.i18n('historyEmbeddingsHeading', this.searchQuery);
  }

  protected getHeadingTextForAnswerSection_(): string {
    if (this.loadingAnswer_) {
      return this.i18n('historyEmbeddingsAnswerLoadingHeading');
    }

    return this.i18n('historyEmbeddingsAnswerHeading');
  }

  protected getAnswerDateTime_(): string {
    if (!this.answerSource_) {
      return '';
    }
    const dateTime = this.getDateTime_(this.answerSource_);
    return this.i18n('historyEmbeddingsAnswerSourceDate', dateTime);
  }

  protected getDateTime_(item: SearchResultItem): string {
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

  protected isAnswerErrorState_(): boolean {
    if (!this.searchResult_) {
      return false;
    }

    return this.searchResult_.answerStatus === AnswerStatus.kExecutionFailure;
  }

  protected onFeedbackSelectedOptionChanged_(
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

  protected onAnswerLinkContextMenu_(e: MouseEvent) {
    this.fire('answer-context-menu', {
      item: this.answerSource_,
      x: e.clientX,
      y: e.clientY,
    });
  }

  protected onAnswerLinkClick_(e: MouseEvent) {
    this.answerLinkClicked_ = true;
    this.fire('answer-click', {
      item: this.answerSource_,
      middleButton: e.button === 1,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });
  }

  protected onMoreActionsClick_(e: Event) {
    e.preventDefault();
    e.stopPropagation();

    assert(this.searchResult_);
    const target = e.target as HTMLElement;
    const index = Number(target.dataset['index']);
    const item = this.searchResult_.items[index];
    assert(item);
    this.actionMenuItem_ = item;
    this.$.sharedMenu.get().showAt(target);
  }

  protected onMoreFromSiteClick_() {
    assert(this.actionMenuItem_);
    this.fire('more-from-site-click', this.actionMenuItem_);
    this.$.sharedMenu.get().close();
  }

  protected async onRemoveFromHistoryClick_() {
    assert(this.searchResult_);
    assert(this.actionMenuItem_);

    this.searchResult_.items.splice(
        this.searchResult_.items.indexOf(this.actionMenuItem_), 1);
    this.searchResultDirty_ = true;
    await this.updateComplete;
    this.fire('remove-item-click', this.actionMenuItem_);
    this.$.sharedMenu.get().close();
  }

  protected onResultContextMenu_(e: MouseEvent) {
    assert(this.searchResult_);
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    this.fire('result-context-menu', {
      item: this.searchResult_.items[index],
      x: e.clientX,
      y: e.clientY,
    });
  }

  protected onResultClick_(e: MouseEvent) {
    assert(this.searchResult_);
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    this.fire('result-click', {
      item: this.searchResult_.items[index],
      middleButton: e.button === 1,
      altKey: e.altKey,
      ctrlKey: e.ctrlKey,
      metaKey: e.metaKey,
      shiftKey: e.shiftKey,
    });

    this.fire('record-history-link-click', {
      resultType: HistoryResultType.EMBEDDINGS,
      index,
    });

    this.clickedIndices_.add(index);
    this.browserProxy_.recordSearchResultsMetrics(
        /* nonEmptyResults= */ true, /* userClickedResult= */ true,
        /* answerShown= */ this.hasAnswer_(),
        /* answerCitationClicked= */ this.answerLinkClicked_,
        /* otherHistoryResultClicked= */ this.otherHistoryResultClicked,
        /* queryWordCount= */ this.searchQuery.split(' ').length);
  }

  private onSearchQueryChanged_() {
    // Flush any old results metrics before overwriting the member variable.
    this.flushDebouncedUserMetrics_();
    this.clickedIndices_.clear();
    this.answerLinkClicked_ = false;

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

    const isNewQuery = this.searchResult_?.query !== result.query;
    const hasResults = result.items.length > 0;
    const hasNewResults =
        this.searchResult_?.items.length !== result.items.length;
    const shouldAnnounceForResults =
        (isNewQuery && hasResults) || (!isNewQuery && hasNewResults);

    // Reset feedback state for new results.
    this.feedbackState_ = CrFeedbackOption.UNSPECIFIED;
    this.searchResult_ = result;
    this.loadingResults_ = false;
    this.loadingAnswer_ = result.answerStatus === AnswerStatus.kLoading;

    this.resultPendingMetricsTimestamp_ = performance.now();

    if (shouldAnnounceForResults) {
      const resultsLabelId = result.items.length === 1 ?
          'historyEmbeddingsMatch' :
          'historyEmbeddingsMatches';
      const message = loadTimeData.getStringF(
          'foundSearchResults', result.items.length,
          loadTimeData.getString(resultsLabelId), result.query);
      getAnnouncerInstance().announce(message);
    }
  }

  protected showAnswerSection_(): boolean {
    if (!this.searchResult_) {
      // If there is no search result yet, the search has just started and it
      // is not yet known if an answer is being attempted.
      return false;
    } else if (this.searchResult_.query !== this.searchQuery) {
      // The current search result and its answer is outdated.
      return false;
    } else {
      // These answer statuses indicate there is no answer to show and no
      // loading state to show.
      return this.searchResult_.answerStatus !== AnswerStatus.kUnspecified &&
          this.searchResult_.answerStatus !== AnswerStatus.kUnanswerable &&
          this.searchResult_.answerStatus !== AnswerStatus.kFiltered &&
          this.searchResult_.answerStatus !== AnswerStatus.kModelUnavailable;
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
      this.browserProxy_.recordSearchResultsMetrics(
          nonEmptyResults, /* userClickedResult= */ false,
          /* answerShown= */ this.hasAnswer_(),
          /* answerCitationClicked= */ this.answerLinkClicked_,
          /* otherHistoryResultClicked= */ this.otherHistoryResultClicked,
          /* queryWordCount= */ this.searchQuery.split(' ').length);
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
