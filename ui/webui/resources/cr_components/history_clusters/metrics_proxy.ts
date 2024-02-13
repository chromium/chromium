// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BrowserProxyImpl} from './browser_proxy.js';
import type {URLVisit} from './history_cluster_types.mojom-webui.js';
import {Annotation} from './history_cluster_types.mojom-webui.js';
import type {ClusterAction, RelatedSearchAction, VisitAction} from './history_clusters.mojom-webui.js';
import {VisitType} from './history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides an abstraction layer for logging metrics for
 * mocking in tests.
 */

export interface MetricsProxy {
  recordClusterAction(action: ClusterAction, index: number): void;
  recordRelatedSearchAction(action: RelatedSearchAction, index: number): void;
  recordToggledVisibility(visible: boolean): void;
  recordVisitAction(action: VisitAction, index: number, type: VisitType): void;
}

export class MetricsProxyImpl implements MetricsProxy {
  recordClusterAction(action: ClusterAction, index: number) {
    BrowserProxyImpl.getInstance().handler.recordClusterAction(action, index);
  }

  recordRelatedSearchAction(action: RelatedSearchAction, index: number) {
    BrowserProxyImpl.getInstance().handler.recordRelatedSearchAction(
        action, index);
  }

  recordToggledVisibility(visible: boolean) {
    BrowserProxyImpl.getInstance().handler.recordToggledVisibility(visible);
  }

  recordVisitAction(action: VisitAction, index: number, type: VisitType) {
    BrowserProxyImpl.getInstance().handler.recordVisitAction(
        action, index, type);
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
        VisitType.kSRP :
        VisitType.kNonSRP;
  }
}

let instance: MetricsProxy|null = null;
