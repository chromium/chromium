// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster.js';
import './history_clusters_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {Time} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {IronScrollThresholdElement} from 'chrome://resources/polymer/v3_0/iron-scroll-threshold/iron-scroll-threshold.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './clusters.html.js';
import {Cluster, URLVisit} from './history_cluster_types.mojom-webui.js';
import {PageCallbackRouter, PageHandlerRemote, QueryResult} from './history_clusters.mojom-webui.js';

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

const HistoryClustersElementBase = I18nMixin(PolymerElement);

export interface HistoryClustersElement {
  $: {
    clusters: IronListElement,
    confirmationDialog: CrLazyRenderElement<CrDialogElement>,
    confirmationToast: CrLazyRenderElement<CrToastElement>,
    scrollThreshold: IronScrollThresholdElement,
  };
}

export class HistoryClustersElement extends HistoryClustersElementBase {
  static get is() {
    return 'history-clusters';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the clusters are in the side panel.
       */
      inSidePanel_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('inSidePanel'),
        reflectToAttribute: true,
      },

      /**
       * The current query for which related clusters are requested and shown.
       */
      query: {
        type: String,
        observer: 'onQueryChanged_',
        value: '',
      },

      /**
       * The placeholder text to show when the results are empty.
       */
      placeholderText_: {
        type: String,
        computed: `computePlaceholderText_(result_.*)`,
      },

      /**
       * The browser response to a request for the freshest clusters related to
       * a given query until an optional given end time (or the present time).
       */
      result_: Object,

      /**
       * Boolean determining if spinner shows instead of load more button.
       */
      showSpinner_: {
        type: Boolean,
        value: false,
      },

      /**
       * The list of visits to be removed. A non-empty array indicates a pending
       * remove request to the browser.
       */
      visitsToBeRemoved_: {
        type: Object,
        value: () => [],
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  query: string;
  private callbackRouter_: PageCallbackRouter;
  private headerText_: string;
  private inSidePanel_: boolean;
  private onClustersQueryResultListenerId_: number|null = null;
  private onClusterImageUpdatedListenerId_: number|null = null;
  private onVisitsRemovedListenerId_: number|null = null;
  private onHistoryDeletedListenerId_: number|null = null;
  private onQueryChangedByUserListenerId_: number|null = null;
  private pageHandler_: PageHandlerRemote;
  private placeholderText_: string;
  private result_: QueryResult;
  private showSpinner_: boolean;
  private visitsToBeRemoved_: URLVisit[];

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

    this.$.clusters.notifyResize();
    this.$.clusters.scrollTarget = this;
    this.$.scrollThreshold.scrollTarget = this;

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
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onCancelButtonClick_() {
    this.visitsToBeRemoved_ = [];
    this.$.confirmationDialog.get().close();
  }

  private onConfirmationDialogCancel_() {
    this.visitsToBeRemoved_ = [];
  }

  private onLoadMoreButtonClick_() {
    if (this.result_ && this.result_.canLoadMore) {
      this.showSpinner_ = true;
      // Prevent sending further load-more requests until this one finishes.
      this.set('result_.canLoadMore', false);
      this.pageHandler_.loadMoreClusters(this.result_.query);
    }
  }

  private onRemoveButtonClick_() {
    this.pageHandler_.removeVisits(this.visitsToBeRemoved_).then(() => {
      // The returned promise resolves with whether the request succeeded in the
      // browser. That value may be used to show a toast but is ignored for now.
      // Allow remove requests again.
      this.visitsToBeRemoved_ = [];
    });
    this.$.confirmationDialog.get().close();
  }

  /**
   * Called with `event` received from a visit requesting to be hidden.
   */
  private onHideVisit_(event: CustomEvent<URLVisit>) {
    this.pageHandler_.hideVisits([event.detail]);
  }

  /**
   * Called with `event` received from visits requesting to be hidden.
   */
  private onHideVisits_(event: CustomEvent<URLVisit[]>) {
    this.pageHandler_.hideVisits(event.detail);
  }

  /**
   * Called with `event` received from a cluster requesting to be removed from
   * the list when all its visits have been removed. Contains the cluster index.
   */
  private onRemoveCluster_(event: CustomEvent<number>) {
    const index = event.detail;
    this.splice('result_.clusters', index, 1);
  }

  /**
   * Called with `event` received from a visit requesting to be removed. `event`
   * may contain the related visits of the said visit, if applicable.
   */
  private onRemoveVisits_(event: CustomEvent<URLVisit[]>) {
    // Return early if there is a pending remove request.
    if (this.visitsToBeRemoved_.length) {
      return;
    }

    this.visitsToBeRemoved_ = event.detail;
    if (this.visitsToBeRemoved_.length > 1) {
      this.$.confirmationDialog.get().showModal();
    } else {
      // Bypass the confirmation dialog if removing one visit only.
      this.onRemoveButtonClick_();
    }
  }

  /**
   * Called when the scrollable area has been scrolled nearly to the bottom.
   */
  private onScrolledToBottom_() {
    this.$.scrollThreshold.clearTriggers();

    if (this.shadowRoot!.querySelector(':focus-visible')) {
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

  private computePlaceholderText_(): string {
    if (!this.result_) {
      return '';
    }
    return this.result_.clusters.length ?
        '' :
        loadTimeData.getString(
            this.result_.query ? 'noSearchResults' :
                                 'historyClustersNoResults');
  }

  /**
   * Returns true and hides the button unless we actually have more results to
   * load. Note we don't actually hide this button based on keyboard-focus
   * state. This is because if the user is using the mouse, more clusters are
   * loaded before the user ever gets a chance to see this button.
   */
  private getLoadMoreButtonHidden_(
      _result: QueryResult, _resultClusters: Cluster[],
      _resultCanLoadMore: Time): boolean {
    return !this.result_ || this.result_.clusters.length === 0 ||
        !this.result_.canLoadMore;
  }

  /**
   * Returns whether the given index corresponds to the last cluster.
   */
  private isLastCluster_(index: number): boolean {
    return index === this.result_.clusters.length - 1;
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
    if (result.isContinuation) {
      // Do not replace the existing result when `result` contains a partial
      // set of clusters that should be appended to the existing ones.
      this.push('result_.clusters', ...result.clusters);
      this.set('result_.canLoadMore', result.canLoadMore);
    } else {
      // Scroll to the top when `result` contains a new set of clusters.
      this.scrollTop = 0;
      this.result_ = result;
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
      if (this.scrollHeight <= this.clientHeight && this.result_.canLoadMore) {
        this.onLoadMoreButtonClick_();
      }
    });
    this.showSpinner_ = false;
  }

  /**
   * Called when an image has become available for `clusterIndex`.
   */
  private onClusterImageUpdated_(clusterIndex: number, imageUrl: Url) {
    // TODO(tommycli): Make deletions handle `clusterIndex` properly.
    this.set(`result_.clusters.${clusterIndex}.imageUrl`, imageUrl);
  }

  /**
   * Called when the user entered search query changes. Also used to fetch the
   * initial set of clusters when the page loads.
   */
  private onQueryChanged_() {
    this.onBrowserIdle_().then(() => {
      if (this.result_ && this.result_.canLoadMore) {
        // Prevent sending further load-more requests until this one finishes.
        this.set('result_.canLoadMore', false);
      }
      this.pageHandler_.startQueryClusters(
          this.query.trim(),
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
      this.$.confirmationToast.get().show();
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
    this.dispatchEvent(new CustomEvent('query-changed-by-user', {
      bubbles: true,
      composed: true,
      detail: query,
    }));
  }
}

customElements.define(HistoryClustersElement.is, HistoryClustersElement);
