// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cluster_menu.js';
import './search_query.js';
import './history_clusters_shared_style.css.js';
import './shared_vars.css.js';
import './url_visit.js';
import '../../cr_elements/cr_icons.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from '../../js/assert_ts.js';
import {loadTimeData} from '../../js/load_time_data.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './cluster.html.js';
import {Cluster, ClusterAction, PageCallbackRouter, SearchQuery, URLVisit, VisitAction} from './history_clusters.mojom-webui.js';
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
      inSidePanel_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('inSidePanel'),
        reflectToAttribute: true,
      },

      /**
       * The current query for which related clusters are requested and shown.
       */
      query: String,

      /**
       * Whether the default-hidden visits are visible.
       */
      expanded_: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      /**
       * The default-hidden visits.
       */
      hiddenVisits_: {
        type: Object,
        computed: `computeHiddenVisits_(cluster.visits.*)`,
      },

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
       * The always-visible visits.
       */
      visibleVisits_: {
        type: Object,
        computed: `computeVisibleVisits_(cluster.visits.*)`,
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
  private expanded_: boolean;
  private hiddenVisits_: URLVisit[];
  private inSidePanel_: boolean;
  private onVisitsRemovedListenerId_: number|null = null;
  private unusedLabel_: string;
  private visibleVisits_: URLVisit[];

  //============================================================================
  // Overridden methods
  //============================================================================

  constructor() {
    super();
    this.callbackRouter_ = BrowserProxyImpl.getInstance().callbackRouter;

    // This element receives a tabindex, because it's an iron-list item.
    // However, what we really want to do is to pass that focus onto an
    // eligible child, so we set `delegatesFocus` to true.
    this.attachShadow({mode: 'open', delegatesFocus: true});
  }

  override connectedCallback() {
    super.connectedCallback();
    this.onVisitsRemovedListenerId_ =
        this.callbackRouter_.onVisitsRemoved.addListener(
            this.onVisitsRemoved_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
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

  private onVisitClicked_(event: CustomEvent<URLVisit>) {
    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kVisitClicked, this.index);

    const visit = event.detail;
    MetricsProxyImpl.getInstance().recordVisitAction(
        VisitAction.kClicked, this.getVisitIndex_(visit),
        MetricsProxyImpl.getVisitType(visit));
  }

  private onOpenAllVisits_() {
    const visitsToOpen = this.visibleVisits_;
    // Only try to open the hidden visits if the user actually has
    // expanded the cluster by clicking "Show More".
    if (this.expanded_) {
      visitsToOpen.push(...this.hiddenVisits_);
    }

    BrowserProxyImpl.getInstance().handler.openVisitUrlsInTabGroup(
        visitsToOpen);

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kOpenedInTabGroup, this.index);
  }

  private onRemoveAllVisits_() {
    // Pass event up with new detail of all this cluster's visits.
    this.dispatchEvent(new CustomEvent('remove-visits', {
      bubbles: true,
      composed: true,
      detail: this.cluster.visits,
    }));
  }

  private onRemoveVisit_(event: CustomEvent<URLVisit>) {
    // The actual removal is handled at in clusters.ts. This is just a good
    // place to record the metric.
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

  private onToggleButtonKeyDown_(e: KeyboardEvent) {
    if (e.key !== 'Enter' && e.key !== ' ') {
      return;
    }

    e.stopPropagation();
    e.preventDefault();

    this.onToggleButtonClick_();
  }

  private onToggleButtonClick_() {
    this.expanded_ = !this.expanded_;

    MetricsProxyImpl.getInstance().recordClusterAction(
        ClusterAction.kRelatedVisitsVisibilityToggled, this.index);

    // Dispatch an event to notify the parent elements of a resize. Note that
    // this simple solution only works because the child iron-collapse has
    // animations disabled. Otherwise, it gets an incorrect mid-animation size.
    this.dispatchEvent(new CustomEvent('iron-resize', {
      bubbles: true,
      composed: true,
    }));
  }

  //============================================================================
  // Helper methods
  //============================================================================

  /**
   * Called with the original remove params when the last accepted request to
   * browser to remove visits succeeds. Since the same visit may appear in
   * multiple Clusters, all Clusters receive this callback in order to get a
   * chance to remove their matching visits.
   */
  private onVisitsRemoved_(removedVisits: URLVisit[]) {
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

  private computeHiddenVisits_(): URLVisit[] {
    return this.cluster.visits.filter((visit: URLVisit) => {
      return visit.hidden;
    });
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
          return query && !(this.inSidePanel_ && index > 2);
        });
  }

  private computeVisibleVisits_(): URLVisit[] {
    return this.cluster.visits.filter((visit: URLVisit) => {
      return !visit.hidden;
    });
  }

  private computeImageUrl_(): string {
    if (!this.cluster.imageUrl) {
      return '';
    }

    // iron-list can't handle our size changing because of loading an image
    // without an explicit event. But we also can't send this until we have
    // updated the image property, so send it on the next idle.
    window.requestIdleCallback(() => {
      this.dispatchEvent(new CustomEvent('iron-resize', {
        bubbles: true,
        composed: true,
      }));
    });

    return this.cluster.imageUrl.url;
  }

  /**
   * Returns the label of the toggle button based on whether the default-hidden
   * visits are visible.
   */
  private getToggleButtonLabel_(_expanded: boolean): string {
    return loadTimeData.getString(
        this.expanded_ ? 'toggleButtonLabelLess' : 'toggleButtonLabelMore');
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
