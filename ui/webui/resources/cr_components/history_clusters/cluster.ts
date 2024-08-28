// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster_menu.js';
import './horizontal_carousel.js';
import './search_query.js';
import './url_visit.js';
import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {HistoryResultType} from '//resources/cr_components/history/constants.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getCss} from './cluster.css.js';
import {getHtml} from './cluster.html.js';
import type {Cluster, SearchQuery, URLVisit} from './history_cluster_types.mojom-webui.js';
import type {PageCallbackRouter} from './history_clusters.mojom-webui.js';
import {ClusterAction, VisitAction} from './history_clusters.mojom-webui.js';
import {MetricsProxyImpl} from './metrics_proxy.js';
import {insertHighlightedTextWithMatchesIntoElement} from './utils.js';

/**
 * @fileoverview This file provides a custom element displaying a cluster.
 */

declare global {
  interface HTMLElementTagNameMap {
    'history-cluster': ClusterElement;
  }
}

const ClusterElementBase = I18nMixinLit(CrLitElement);

export interface ClusterElement {
  $: {
    label: HTMLElement,
    container: HTMLElement,
  };
}

export class ClusterElement extends ClusterElementBase {
  static get is() {
    return 'history-cluster';
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
       * The cluster displayed by this element.
       */
      cluster: {type: Object},

      /**
       * The index of the cluster.
       */
      index: {type: Number},

      /**
       * Whether the cluster is in the side panel.
       */
      inSidePanel: {
        type: Boolean,
        reflect: true,
      },

      /**
       * The current query for which related clusters are requested and shown.
       */
      query: {type: String},

      /**
       * The visible related searches.
       */
      relatedSearches_: {type: Array},

      /**
       * The label for the cluster. This property is actually unused. The side
       * effect of the compute function is used to insert the HTML elements for
       * highlighting into this.$.label element.
       */
      label_: {
        type: String,
        state: true,
      },

      /**
       * The cluster's image URL in a form easily passed to cr-auto-img.
       * Also notifies the outer iron-list of a resize.
       */
      imageUrl_: {type: String},
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  cluster?: Cluster;
  index: number = -1;  // Initialized to an invalid value.
  inSidePanel: boolean = loadTimeData.getBoolean('inSidePanel');
  query: string = '';
  protected imageUrl_: string = '';
  protected relatedSearches_: SearchQuery[] = [];

  private callbackRouter_: PageCallbackRouter;
  private onVisitsHiddenListenerId_: number|null = null;
  private onVisitsRemovedListenerId_: number|null = null;
  private label_: string = 'no_label';

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;
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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('cluster')) {
      assert(this.cluster);
      this.label_ = this.cluster.label ? this.cluster.label : 'no_label';
      this.imageUrl_ = this.cluster.imageUrl ? this.cluster.imageUrl.url : '';
      this.relatedSearches_ = this.cluster.relatedSearches.filter(
          (query: SearchQuery, index: number) => {
            return query && !(this.inSidePanel && index > 2);
          });
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('label_') && this.label_ !== 'no_label' &&
        this.cluster) {
      insertHighlightedTextWithMatchesIntoElement(
          this.$.label, this.cluster.label!, this.cluster.labelMatchPositions);
    }
    if (changedPrivateProperties.has('imageUrl_')) {
      // iron-list can't handle our size changing because of loading an image
      // without an explicit event. But we also can't send this until we have
      // updated the image property, so send it on the next idle.
      requestIdleCallback(() => {
        this.fire('iron-resize');
      });
    } else if (changedProperties.has('cluster')) {
      // Iron-list re-assigns the `cluster` property to reuse existing elements
      // as the user scrolls. Since this property can change the height of this
      // element, we need to notify iron-list that this element's height may
      // need to be re-calculated.
      this.fire('iron-resize');
    }
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onRelatedSearchClicked_() {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kRelatedSearchClicked, this.index);
  }

  /* Clears selection on non alt mouse clicks. Need to wait for browser to
   *  update the DOM fully. */
  protected clearSelection_(event: MouseEvent) {
    this.onBrowserIdle_().then(() => {
      if (window.getSelection() && !event.altKey) {
        window.getSelection()?.empty();
      }
    });
  }

  protected onVisitClicked_(event: CustomEvent<URLVisit>) {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kVisitClicked, this.index);

    const visit = event.detail;
    const visitIndex = this.getVisitIndex_(visit);
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.kClicked, visitIndex, MetricsProxyImpl.getVisitType(visit));

    this.fire('record-history-link-click', {
      resultType: HistoryResultType.GROUPED,
      index: visitIndex,
    });
  }

  protected onOpenAllVisits_() {
    assert(this.cluster);
    BrowserProxyImpl.getInstance().handler.openVisitUrlsInTabGroup(
        this.cluster.visits, this.cluster.tabGroupName ?? null);

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kOpenedInTabGroup, this.index);
  }

  protected onHideAllVisits_() {
    this.fire('hide-visits', this.cluster ? this.cluster.visits : []);
  }

  protected onRemoveAllVisits_() {
    // Pass event up with new detail of all this cluster's visits.
    this.fire('remove-visits', this.cluster ? this.cluster.visits : []);
  }

  protected onHideVisit_(event: CustomEvent<URLVisit>) {
    // The actual hiding is handled in clusters.ts. This is just a good place to
    // record the metric.
    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.kHidden, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));
  }

  protected onRemoveVisit_(event: CustomEvent<URLVisit>) {
    // The actual removal is handled in clusters.ts. This is just a good place
    // to record the metric.
    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.kDeleted, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));

    this.fire('remove-visits', [visit]);
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
    assert(this.cluster);
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
      this.fire('remove-cluster', this.index);

      MetricsProxyImpl.getInstance().recordClusterAction(
          ClusterAction.kDeleted, this.index);
    } else {
      this.cluster.visits = remainingVisits;
      this.requestUpdate();
    }

    this.updateComplete.then(() => {
      this.fire('iron-resize');
    });
  }

  /**
   * Returns the index of `visit` among the visits in the cluster. Returns -1
   * if the visit is not found in the cluster at all.
   */
  private getVisitIndex_(visit: URLVisit): number {
    return this.cluster ? this.cluster.visits.indexOf(visit) : -1;
  }

  protected hideRelatedSearches_(): boolean {
    return !this.cluster || !this.cluster.relatedSearches.length;
  }

  protected debugInfo_(): string {
    return this.cluster && this.cluster.debugInfo ? this.cluster.debugInfo : '';
  }

  protected timestamp_(): string {
    return this.cluster && this.cluster.visits.length > 0 ?
        this.cluster.visits[0]!.relativeDate :
        '';
  }

  protected visits_(): URLVisit[] {
    return this.cluster ? this.cluster.visits : [];
  }
}

customElements.define(ClusterElement.is, ClusterElement);
