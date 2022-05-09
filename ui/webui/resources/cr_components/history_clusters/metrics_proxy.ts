// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Annotation, URLVisit} from './history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides an abstraction layer for logging metrics for
 * mocking in tests.
 */

/**
 * The following enums must be kept in sync with their respective variants in
 * //tools/metrics/histograms/metadata/history/histograms.xml
 */
export enum ClusterAction {
  DELETED = 'Deleted',
  OPENED_IN_TAB_GROUP = 'OpenedInTabGroup',
  RELATED_SEARCH_CLICKED = 'RelatedSearchClicked',
  RELATED_VISITS_VISIBILITY_TOGGLED = 'RelatedVisitsVisibilityToggled',
  VISIT_CLICKED = 'VisitClicked',
}

export enum RelatedSearchAction {
  CLICKED = 'Clicked',
}

export enum VisitAction {
  CLICKED = 'Clicked',
  DELETED = 'Deleted',
}

export enum VisitType {
  NON_SRP = 'nonSRP',
  SRP = 'SRP',
}

export interface MetricsProxy {
  recordClusterAction(action: ClusterAction, index: number): void;
  recordRelatedSearchAction(action: RelatedSearchAction, index: number): void;
  recordToggledVisibility(visible: boolean): void;
  recordVisitAction(action: VisitAction, index: number, type: VisitType): void;
}

export class MetricsProxyImpl implements MetricsProxy {
  recordClusterAction(action: ClusterAction, index: number) {
    chrome.metricsPrivate.recordMediumCount(
        `History.Clusters.UIActions.Cluster.${action}`, index);
  }

  recordRelatedSearchAction(action: RelatedSearchAction, index: number) {
    chrome.metricsPrivate.recordMediumCount(
        `History.Clusters.UIActions.RelatedSearch.${action}`, index);
  }

  recordToggledVisibility(visible: boolean) {
    chrome.metricsPrivate.recordBoolean(
        'History.Clusters.UIActions.ToggledVisibility', visible);
  }

  recordVisitAction(action: VisitAction, index: number, type: VisitType) {
    chrome.metricsPrivate.recordMediumCount(
        `History.Clusters.UIActions.Visit.${action}`, index);
    chrome.metricsPrivate.recordMediumCount(
        `History.Clusters.UIActions.${type}Visit.${action}`, index);
  }

  static getInstance(): MetricsProxy {
    return instance || (instance = new MetricsProxyImpl());
  }

  static setInstance(obj: MetricsProxy) {
    instance = obj;
  }

  /**
   * Returns the VisitType based on whether this is a visit to the default
   * search provider's results page.
   */
  static getVisitType(visit: URLVisit): VisitType {
    return visit.annotations.includes(Annotation.kSearchResultsPage) ?
        VisitType.SRP :
        VisitType.NON_SRP;
  }
}

let instance: MetricsProxy|null = null;
