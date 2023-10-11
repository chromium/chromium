// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster_menu.js';
import './history_clusters_shared_style.css.js';
import './horizontal_carousel.js';
import './search_query.js';
import './shared_vars.css.js';
import './url_visit.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './cluster.html.js';
import {Cluster, SearchQuery, URLVisit} from './history_cluster_types.mojom-webui.js';
import {ClusterAction, PageCallbackRouter, VisitAction} from './history_clusters.mojom-webui.js';
import {MetricsProxyImpl} from './metrics_proxy.js';
import {insertHighlightedTextWithMatchesIntoElement} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'history-cluster': HistoryClusterElement;
  }
}

const HistoryClusterElementBase = I18nMixin(PolymerElement);

interface HistoryClusterElement {
  $: {
    label: HTMLElement,
    container: HTMLElement,
  };
}

class HistoryClusterElement extends HistoryClusterElementBase {
  static get is() {
    return 'history-cluster';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The cluster displayed by this element.
       */
      cluster: Object,

      /**
       * The index of the cluster.
       */
      index: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * Whether the cluster is in the side panel.
       */
      inSidePanel: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('inSidePanel'),
        reflectToAttribute: true,
      },

      /**
       * The current query for which related clusters are requested and shown.
       */
      query: String,

      /**
       * The visible related searches.
       */
      relatedSearches_: {
        type: Object,
        computed: `computeRelatedSearches_(cluster.relatedSearches.*)`,
      },

      /**
       * The label for the cluster. This property is actually unused. The side
       * effect of the compute function is used to insert the HTML elements for
       * highlighting into this.$.label element.
       */
      unusedLabel_: {
        type: String,
        computed: 'computeLabel_(cluster.label)',
      },

      /**
       * The cluster's image URL in a form easily passed to cr-auto-img.
       * Also notifies the outer iron-list of a resize.
       */
      imageUrl_: {
        type: String,
        computed: `computeImageUrl_(cluster.imageUrl)`,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  cluster: Cluster;
  index: number;
  query: string;
  private callbackRouter_: PageCallbackRouter;

  inSidePanel: boolean;
  private onVisitsHiddenListenerId_: number|null = null;
  private onVisitsRemovedListenerId_: number|null = null;
  private unusedLabel_: string;

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;

    // This element receives a tabindex, because it's an iron-list item.
    // However, what we really want to do is to pass that focus onto an
    // eligible child, so we want to set `delegatesFocus` to true. But
    // delegatesFocus removes the text selection. So temporarily removing
    // the delegatesFocus until that issue is fixed.
  }

  override connectedCallback() {
    super.connectedCallback();
    this.onVisitsHiddenListenerId_ =
        this.callbackRouter_.onVisitsHidden.addListener(
            this.onVisitsRemovedOrHidden_.bind(this));
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemovedOrHidden_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.onVisitsHiddenListenerId_);
    this.callbackRouter_.removeListener(this.onVisitsHiddenListenerId_);
    this.onVisitsHiddenListenerId_ = null;

    assert(this.onVisitsRemovedListenerId_);
    this.callbackRouter_.removeListener(this.onVisitsRemovedListenerId_);
    this.onVisitsRemovedListenerId_ = null;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  private onRelatedSearchClicked_() {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kRelatedSearchClicked, this.index);
  }

  /* Clears selection on non alt mouse clicks. Need to wait for browser to
   *  update the DOM fully. */
  private clearSelection_(event: MouseEvent) {
    this.onBrowserIdle_().then(() => {
      if (window.getSelection() && !event.altKey) {
        window.getSelection()?.empty();
      }
    });
  }

  private onVisitClicked_(event: CustomEvent<URLVisit>) {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kVisitClicked, this.index);

    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.kClicked, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));
  }

  private onOpenAllVisits_() {
    BrowserProxyImpl.getInstance().handler.openVisitUrlsInTabGroup(
        this.cluster.visits, this.cluster.tabGroupName ?? null);

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kOpenedInTabGroup, this.index);
  }

  private onHideAllVisits_() {
    this.dispatchEvent(new CustomEvent('hide-visits', {
      bubbles: true,
      composed: true,
      detail: this.cluster.visits,
    }));
  }

  private onRemoveAllVisits_() {
    // Pass event up with new detail of all this cluster's visits.
    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: this.cluster.visits,
    }));
  }

  private onHideVisit_(event: CustomEvent<URLVisit>) {
    // The actual hiding is handled in clusters.ts. This is just a good place to
    // record the metric.
    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.kHidden, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));
  }

  private onRemoveVisit_(event: CustomEvent<URLVisit>) {
    // The actual removal is handled in clusters.ts. This is just a good place
    // to record the metric.
    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.kDeleted, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));

    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: [visit],
    }));
  }

  //============================================================================
  // Helper methods
  //============================================================================

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

  /**
   * Called with the original remove or hide params when the last accepted
   * request to browser to remove or hide visits succeeds. Since the same visit
   * may appear in multiple Clusters, all Clusters receive this callback in
   * order to get a chance to remove their matching visits.
   */
  private onVisitsRemovedOrHidden_(removedVisits: URLVisit[]) {
    const visitHasBeenRemoved = (visit: URLVisit) => {
      return removedVisits.findIndex((removedVisit) => {
        if (visit.normalizedUrl.url !== removedVisit.normalizedUrl.url) {
          return false;
        }

        // Remove the visit element if any of the removed visit's raw timestamps
        // matches the canonical raw timestamp.
        const rawVisitTime = visit.rawVisitData.visitTime.internalValue;
        return (removedVisit.rawVisitData.visitTime.internalValue ===
                rawVisitTime) ||
            removedVisit.duplicates.map(data => data.visitTime.internalValue)
                .includes(rawVisitTime);
      }) !== -1;
    };

    const allVisits = this.cluster.visits;
    const remainingVisits = allVisits.filter(v => !visitHasBeenRemoved(v));
    if (allVisits.length === remainingVisits.length) {
      return;
    }

    if (!remainingVisits.length) {
      // If all the visits are removed, fire an event to also remove this
      // cluster from the list of clusters.
      this.dispatchEvent(new CustomEvent('remove-cluster', {
        bubbles: true,
        composed: true,
        detail: this.index,
      }));

      MetricsProxyImpl.getInstance().recordClusterAction(
          ClusterAction.kDeleted, this.index);
    } else {
      this.set('cluster.visits', remainingVisits);
    }

    this.dispatchEvent(new CustomEvent('iron-resize', {
      bubbles: true,
      composed: true,
    }));
  }

  private computeLabel_(): string {
    if (!this.cluster.label) {
      // This never happens unless we misconfigured our variations config.
      // This sentinel string matches the Android UI.
      return 'no_label';
    }

    insertHighlightedTextWithMatchesIntoElement(
        this.$.label, this.cluster.label!, this.cluster.labelMatchPositions);
    return this.cluster.label!;
  }

  private computeRelatedSearches_(): SearchQuery[] {
    return this.cluster.relatedSearches.filter(
        (query: SearchQuery, index: number) => {
          return query && !(this.inSidePanel && index > 2);
        });
  }

  private computeImageUrl_(): string {
    if (!this.cluster.imageUrl) {
      return '';
    }

    // iron-list can't handle our size changing because of loading an image
    // without an explicit event. But we also can't send this until we have
    // updated the image property, so send it on the next idle.
    requestIdleCallback(() => {
      this.dispatchEvent(new CustomEvent('iron-resize', {
        bubbles: true,
        composed: true,
      }));
    });

    return this.cluster.imageUrl.url;
  }

  /**
   * Returns the index of `visit` among the visits in the cluster. Returns -1
   * if the visit is not found in the cluster at all.
   */
  private getVisitIndex_(visit: URLVisit): number {
    return this.cluster.visits.indexOf(visit);
  }
}

customElements.define(HistoryClusterElement.is, HistoryClusterElement);
