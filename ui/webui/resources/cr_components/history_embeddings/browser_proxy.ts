// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote, SearchQuery, UserFeedback} from './history_embeddings.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from './history_embeddings.mojom-webui.js';

export interface HistoryEmbeddingsBrowserProxy {
  search(query: SearchQuery): void;
  sendQualityLog(selectedIndices: number[], numCharsForQuery: number): void;
  recordSearchResultsMetrics(
      nonEmptyResults: boolean, userClickedResult: boolean): void;
  setUserFeedback(userFeedback: UserFeedback): void;
  maybeShowFeaturePromo(): void;
  openSettingsPage(): void;

  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
}

export class HistoryEmbeddingsBrowserProxyImpl implements
    HistoryEmbeddingsBrowserProxy {
  static instance: HistoryEmbeddingsBrowserProxy|null = null;
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor(handler: PageHandlerRemote, callbackRouter?: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter || new PageCallbackRouter();
  }

  static getInstance(): HistoryEmbeddingsBrowserProxy {
    if (HistoryEmbeddingsBrowserProxyImpl.instance) {
      return HistoryEmbeddingsBrowserProxyImpl.instance;
    }
    const handler = PageHandler.getRemote();
    const callbackRouter = new PageCallbackRouter();
    handler.setPage(callbackRouter.$.bindNewPipeAndPassRemote());
    HistoryEmbeddingsBrowserProxyImpl.instance =
        new HistoryEmbeddingsBrowserProxyImpl(handler, callbackRouter);
    return HistoryEmbeddingsBrowserProxyImpl.instance;
  }

  static setInstance(newInstance: HistoryEmbeddingsBrowserProxy) {
    HistoryEmbeddingsBrowserProxyImpl.instance = newInstance;
  }

  search(query: SearchQuery) {
    this.handler.search(query);
  }

  sendQualityLog(selectedIndices: number[], numCharsForQuery: number) {
    return this.handler.sendQualityLog(selectedIndices, numCharsForQuery);
  }

  recordSearchResultsMetrics(
      nonEmptyResults: boolean, userClickedResult: boolean) {
    this.handler.recordSearchResultsMetrics(nonEmptyResults, userClickedResult);
  }

  setUserFeedback(userFeedback: UserFeedback) {
    this.handler.setUserFeedback(userFeedback);
  }

  maybeShowFeaturePromo() {
    this.handler.maybeShowFeaturePromo();
  }

  openSettingsPage() {
    this.handler.openSettingsPage();
  }
}
