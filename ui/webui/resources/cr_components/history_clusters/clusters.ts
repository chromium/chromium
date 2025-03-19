// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster.js';
import './history_clusters_shared_style.css.js';
import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import '//resources/cr_elements/cr_toast/cr_toast.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrInfiniteListElement} from '//resources/cr_elements/cr_infinite_list/cr_infinite_list.js';
import type {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import type {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './clusters.css.js';
import {getHtml} from './clusters.html.js';
import type {Cluster, URLVisit} from './history_cluster_types.mojom-webui.js';
import type {PageCallbackRouter, PageHandlerRemote, QueryResult} from './history_clusters.mojom-webui.js';

function jsDateToMojoDate(date: Date): Time {
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const internalValue = BigInt(date.valueOf() + epochDeltaInMs) * BigInt(1000);
  return {internalValue};
}

/**
 * @fileoverview This file provides a custom element that requests and shows
 * history clusters given a query. It handles loading more clusters using
 * infinite scrolling as well as deletion of visits within the clusters.
 */

declare global {
  interface HTMLElementTagNameMap {
    'history-clusters': HistoryClustersElement;
  }
}

const HistoryClustersElementBase = I18nMixinLit(CrLitElement);

export interface HistoryClustersElement {
  $: {
    clusters: CrInfiniteListElement,
    confirmationToast: CrToastElement,
  };
}

export class HistoryClustersElement extends HistoryClustersElementBase {
  static get is() {
    return 'history-clusters';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {

      /**
       * Whether the clusters are in the side panel.
       */
      inSidePanel_: {
        type: Boolean,
        reflect: true,
      },

      /**
       * The current query for which related clusters are requested and shown.
       */
      query: {type: String},
      timeRangeStart: {type: Object},


      /**
       * These 3 properties are components of the browser response to a request
       * for the freshest clusters related to  a given query until an optional
       * given end time (or the present time).
       */
      canLoadMore_: {type: Boolean},
      clusters_: {type: Array},
      hasResult_: {type: Boolean},
      resultQuery_: {type: String},

      /**
       * Boolean determining if spinner shows instead of load more button.
       */
      showSpinner_: {type: Boolean},
      showConfirmationDialog_: {type: Boolean},

      /**
       * The list of visits to be removed. A non-empty array indicates a pending
       * remove request to the browser.
       */
      visitsToBeRemoved_: {type: Array},

      scrollOffset: {type: Number},
      scrollTarget: {type: Object},

      // Whether this element is active, i.e. visible to the user. Defaults to
      // true. Clients should set to false when this element is inactive,
      // e.g. in a tabbed UI when the active tab doesn't contain this element.
      isActive: {
        type: Boolean,
        reflect: true,
      },

      isEmpty: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================
  accessor isActive: boolean = true;
  accessor isEmpty: boolean = true;
  accessor query: string = '';
  accessor scrollOffset: number = 0;
  accessor scrollTarget: HTMLElement = document.documentElement;
  accessor timeRangeStart: Date|undefined;
  protected accessor canLoadMore_: boolean = false;
  protected accessor clusters_: Cluster[] = [];
  protected accessor hasResult_: boolean = false;
  protected accessor resultQuery_: string = '';
  private callbackRouter_: PageCallbackRouter;
  private accessor inSidePanel_: boolean =
      loadTimeData.getBoolean('inSidePanel');
  private lastOffsetHeight_: number = 0;
  private resizeObserver_: ResizeObserver = new ResizeObserver(() => {
    if (this.lastOffsetHeight_ === 0) {
      this.lastOffsetHeight_ = this.scrollTarget.offsetHeight;
      return;
    }
    if (this.scrollTarget.offsetHeight > this.lastOffsetHeight_) {
      this.lastOffsetHeight_ = this.scrollTarget.offsetHeight;
      this.onScrollOrResize_();
    }
  });
  private scrollDebounce_: number = 200;
  private scrollListener_: EventListener = () => this.onScrollOrResize_();
  private onClustersQueryResultListenerId_: number|null = null;
  private onClusterImageUpdatedListenerId_: number|null = null;
  private onVisitsRemovedListenerId_: number|null = null;
  private onHistoryDeletedListenerId_: number|null = null;
  private onQueryChangedByUserListenerId_: number|null = null;
  private pageHandler_: PageHandlerRemote;
  protected accessor showConfirmationDialog_: boolean = false;
  protected accessor showSpinner_: boolean = false;
  private scrollTimeout_: number|null = null;
  private accessor visitsToBeRemoved_: URLVisit[] = [];

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.pageHandler_ = BrowserProxyImpl.getInstance().handler;
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();

    // Register a per-document singleton focus outline manager. Some of our
    // child elements depend on the CSS classes set by this singleton.
    FocusOutlineManager.forDocument(document);

    this.onClustersQueryResultListenerId_ =
        this.callbackRouter_.onClustersQueryResult.addListener(
            this.onClustersQueryResult_.bind(this));
    this.onClusterImageUpdatedListenerId_ =
        this.callbackRouter_.onClusterImageUpdated.addListener(
            this.onClusterImageUpdated_.bind(this));
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
    this.onHistoryDeletedListenerId_ =
        this.callbackRouter_.onHistoryDeleted.addListener(
            this.onHistoryDeleted_.bind(this));
    this.onQueryChangedByUserListenerId_ =
        this.callbackRouter_.onQueryChangedByUser.addListener(
            this.onQueryChangedByUser_.bind(this));

    if (this.inSidePanel_) {
      this.pageHandler_.showSidePanelUI();
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.onClustersQueryResultListenerId_);
    this.callbackRouter_.removeListener(this.onClustersQueryResultListenerId_);
    this.onClustersQueryResultListenerId_ = null;
    assert(this.onVisitsRemovedListenerId_);
    this.callbackRouter_.removeListener(this.onVisitsRemovedListenerId_);
    this.onVisitsRemovedListenerId_ = null;
    assert(this.onHistoryDeletedListenerId_);
    this.callbackRouter_.removeListener(this.onHistoryDeletedListenerId_);
    this.onHistoryDeletedListenerId_ = null;
    assert(this.onQueryChangedByUserListenerId_);
    this.callbackRouter_.removeListener(this.onQueryChangedByUserListenerId_);
    this.onQueryChangedByUserListenerId_ = null;
    assert(this.onClusterImageUpdatedListenerId_);
    this.callbackRouter_.removeListener(this.onClusterImageUpdatedListenerId_);
    this.onClusterImageUpdatedListenerId_ = null;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('query') ||
        changedProperties.has('timeRangeStart')) {
      this.onQueryChanged_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('hasResult_') ||
        changedPrivateProperties.has('clusters_')) {
      this.isEmpty = this.hasResult_ && this.clusters_.length === 0;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('scrollTarget')) {
      const oldTarget = changedProperties.get('scrollTarget');
      // Remove old listeners. ResizeObserver always listens, scroll observer
      // is only on when this element is active.
      if (oldTarget) {
        this.resizeObserver_.disconnect();
        // Also remove the scroll listener if the element is currently active
        // or was active and changed to inactive this lifecycle.
        if (this.isActive || changedProperties.has('isActive')) {
          oldTarget.removeEventListener('scroll', this.scrollListener_);
        }
      }
      if (this.scrollTarget) {
        this.resizeObserver_.observe(this.scrollTarget);
        if (this.isActive) {
          this.scrollTarget.addEventListener('scroll', this.scrollListener_);
        }
      }
    } else if (changedProperties.has('isActive')) {
      if (this.isActive) {
        // Active changed from false to true. Add the scroll observer.
        this.scrollTarget.addEventListener('scroll', this.scrollListener_);
      } else {
        // Active changed from true to false. Remove scroll observer.
        this.scrollTarget.removeEventListener('scroll', this.scrollListener_);
      }
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('clusters_')) {
      const previous =
          changedPrivateProperties.get('clusters_') as (Cluster[] | undefined);
      const clustersRemoved =
          previous && (previous.length > this.clusters_.length);
      if (clustersRemoved && this.canLoadMore_ &&
          this.$.clusters.offsetHeight < this.scrollTarget.offsetHeight) {
        this.onLoadMoreButtonClick_();
      }
    }
  }

  //============================================================================
  // Event handlers
  //============================================================================
  protected onCancelButtonClick_() {
    this.visitsToBeRemoved_ = [];
    this.getConfirmationDialog_().close();
  }

  protected onConfirmationDialogCancel_() {
    this.visitsToBeRemoved_ = [];
  }

  protected onLoadMoreButtonClick_() {
    if (this.hasResult_ && this.canLoadMore_) {
      this.showSpinner_ = true;
      // Prevent sending further load-more requests until this one finishes.
      this.canLoadMore_ = false;
      this.pageHandler_.loadMoreClusters(this.resultQuery_);
    }
  }

  protected onRemoveButtonClick_() {
    this.removeVisits_();
    this.getConfirmationDialog_().close();
  }

  /**
   * Called with `event` received from a visit requesting to be hidden.
   */
  protected onHideVisit_(event: CustomEvent<URLVisit>) {
    this.pageHandler_.hideVisits([event.detail]);
  }

  /**
   * Called with `event` received from visits requesting to be hidden.
   */
  protected onHideVisits_(event: CustomEvent<URLVisit[]>) {
    this.pageHandler_.hideVisits(event.detail);
  }

  /**
   * Called with `event` received from a cluster requesting to be removed from
   * the list when all its visits have been removed. Contains the cluster index.
   */
  protected onRemoveCluster_(event: CustomEvent<number>) {
    const index = event.detail;
    this.clusters_ =
        [...this.clusters_.slice(0, index), ...this.clusters_.slice(index + 1)];
  }

  /**
   * Called with `event` received from a visit requesting to be removed. `event`
   * may contain the related visits of the said visit, if applicable.
   */
  protected async onRemoveVisits_(event: CustomEvent<URLVisit[]>) {
    // Return early if there is a pending remove request.
    if (this.visitsToBeRemoved_.length) {
      return;
    }

    this.visitsToBeRemoved_ = event.detail;

    // Bypass the confirmation dialog if removing one visit only.
    if (this.visitsToBeRemoved_.length === 1) {
      this.removeVisits_();
      return;
    }

    // Show a confirmation dialog.
    if (!this.showConfirmationDialog_) {
      this.showConfirmationDialog_ = true;
      await this.updateComplete;
    }
    this.getConfirmationDialog_().showModal();
  }

  setScrollDebounceForTest(debounce: number) {
    this.scrollDebounce_ = debounce;
  }

  /**
   * Called when the scrollable area has been scrolled nearly to the bottom.
   */
  private onScrolledToBottom_() {
    if (this.shadowRoot.querySelector(':focus-visible')) {
      // If some element of ours is keyboard-focused, don't automatically load
      // more clusters. It loses the user's position and messes up screen
      // readers. Let the user manually click the "Load More" button, if needed.
      // We use :focus-visible here, because :focus is triggered by mouse focus
      // too. And `FocusOutlineManager.visible()` is too primitive. It's true
      // on page load, and whenever the user is typing in the searchbox.
      return;
    }

    this.onLoadMoreButtonClick_();
  }

  //============================================================================
  // Helper methods
  //============================================================================
  private getConfirmationDialog_(): CrDialogElement {
    const dialog = this.shadowRoot.querySelector('cr-dialog');
    assert(dialog);
    return dialog;
  }

  protected computePlaceholderText_(): string {
    if (!this.hasResult_) {
      return '';
    }
    return this.clusters_.length ?
        '' :
        loadTimeData.getString(
            this.resultQuery_ ? 'noSearchResults' : 'historyClustersNoResults');
  }

  /**
   * Returns true and hides the button unless we actually have more results to
   * load. Note we don't actually hide this button based on keyboard-focus
   * state. This is because if the user is using the mouse, more clusters are
   * loaded before the user ever gets a chance to see this button.
   */
  protected getLoadMoreButtonHidden_(): boolean {
    return !this.hasResult_ || this.clusters_.length === 0 ||
        !this.canLoadMore_;
  }

  /**
   * Returns whether the given index corresponds to the last cluster.
   */
  protected isLastCluster_(index: number): boolean {
    return index === this.clusters_.length - 1;
  }

  /**
   * Returns a promise that resolves when the browser is idle.
   */
  private onBrowserIdle_(): Promise<void> {
    return new Promise(resolve => {
      requestIdleCallback(() => {
        resolve();
      });
    });
  }

  private onClustersQueryResult_(result: QueryResult) {
    this.hasResult_ = true;
    this.canLoadMore_ = result.canLoadMore;
    if (result.isContinuation) {
      // Append the new set of clusters to the existing ones, since this is a
      // continuation of the existing clusters.
      this.clusters_ = [...this.clusters_.slice(), ...result.clusters];
    } else {
      // Scroll to the top when `result` contains a new set of clusters.
      this.scrollTarget.scrollTop = 0;
      this.clusters_ = result.clusters;
      this.resultQuery_ = result.query;
    }

    // Handle the "tall monitor" edge case: if the returned results are are
    // shorter than the vertical viewport, the <history-clusters> element will
    // not have a scrollbar, and the user will never be able to trigger the
    // iron-scroll-threshold to request more results. Therefore, immediately
    // request more results if there is no scrollbar to fill the viewport.
    //
    // This should happen quite rarely in the queryless state since the backend
    // transparently tries to get at least ~100 visits to cluster.
    //
    // This is likely to happen very frequently in the search query state, since
    // many clusters will not match the search query and will be discarded.
    //
    // Do this on browser idle to avoid jank and to give the DOM a chance to be
    // updated with the results we just got.
    this.onBrowserIdle_().then(() => {
      if ((this.$.clusters.offsetHeight < this.scrollTarget.offsetHeight ||
           this.scrollTarget.scrollHeight <= this.scrollTarget.clientHeight) &&
          this.canLoadMore_) {
        this.onLoadMoreButtonClick_();
      }
    });
    this.showSpinner_ = false;
  }

  /**
   * Called when an image has become available for `clusterIndex`.
   */
  private onClusterImageUpdated_(clusterIndex: number, imageUrl: Url) {
    const cluster = this.clusters_[clusterIndex];
    const newCluster = Object.assign({}, cluster) as unknown as Cluster;
    newCluster.imageUrl = imageUrl;

    // TODO(tommycli): Make deletions handle `clusterIndex` properly.
    this.clusters_[clusterIndex] = newCluster;
    this.requestUpdate();
  }

  /**
   * Called when the user entered search query changes. Also used to fetch the
   * initial set of clusters when the page loads.
   */
  private onQueryChanged_() {
    this.onBrowserIdle_().then(() => {
      if (this.hasResult_ && this.canLoadMore_) {
        // Prevent sending further load-more requests until this one finishes.
        this.canLoadMore_ = false;
      }
      this.pageHandler_.startQueryClusters(
          this.query.trim(),
          this.timeRangeStart ? jsDateToMojoDate(this.timeRangeStart) : null,
          new URLSearchParams(window.location.search).has('recluster'));
    });
  }

  /**
   * Called with the original remove params when the last accepted request to
   * browser to remove visits succeeds.
   */
  private onVisitsRemoved_(removedVisits: URLVisit[]) {
    // Show the confirmation toast once done removing one visit only; since a
    // confirmation dialog was not shown prior to the action.
    if (removedVisits.length === 1) {
      this.$.confirmationToast.show();
    }
  }

  /**
   * Called when History is deleted from a different tab.
   */
  private onHistoryDeleted_() {
    // Just re-issue the existing query to "reload" the results and display
    // the externally deleted History. It would be nice if we could save the
    // user's scroll position, but History doesn't do that either.
    this.onQueryChanged_();
  }

  /**
   * Called when the query is changed by the user externally.
   */
  private onQueryChangedByUser_(query: string) {
    // Don't directly change the query, but instead let the containing element
    // update the searchbox UI. That in turn will cause this object to issue
    // a new query to the backend.
    this.fire('query-changed-by-user', query);
  }

  private onScrollOrResize_() {
    // Debounce by 200ms.
    if (this.scrollTimeout_) {
      clearTimeout(this.scrollTimeout_);
    }
    this.scrollTimeout_ =
        setTimeout(() => this.onScrollTimeout_(), this.scrollDebounce_);
  }

  private onScrollTimeout_() {
    this.scrollTimeout_ = null;
    const lowerScroll = this.scrollTarget.scrollHeight -
        this.scrollTarget.scrollTop - this.scrollTarget.offsetHeight;
    if (lowerScroll < 500) {
      this.onScrolledToBottom_();
    }
  }

  private removeVisits_() {
    this.pageHandler_.removeVisits(this.visitsToBeRemoved_).then(() => {
      // The returned promise resolves with whether the request succeeded in the
      // browser. That value may be used to show a toast but is ignored for now.
      // Allow remove requests again.
      this.visitsToBeRemoved_ = [];
    });
  }
}

customElements.define(HistoryClustersElement.is, HistoryClustersElement);
