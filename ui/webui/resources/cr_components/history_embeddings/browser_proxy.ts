// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote} from './history_embeddings.mojom-webui.js';
import {PageHandler} from './history_embeddings.mojom-webui.js';

export interface HistoryEmbeddingsBrowserProxy {
  doSomething(): Promise<boolean>;
}

export class HistoryEmbeddingsBrowserProxyImpl implements
    HistoryEmbeddingsBrowserProxy {
  static instance: HistoryEmbeddingsBrowserProxy|null = null;
  handler: PageHandlerRemote;

  constructor(handler: PageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): HistoryEmbeddingsBrowserProxy {
    return HistoryEmbeddingsBrowserProxyImpl.instance ||
        (HistoryEmbeddingsBrowserProxyImpl.instance =
             new HistoryEmbeddingsBrowserProxyImpl(PageHandler.getRemote()));
  }

  static setInstance(newInstance: HistoryEmbeddingsBrowserProxy) {
    HistoryEmbeddingsBrowserProxyImpl.instance = newInstance;
  }

  doSomething() {
    return this.handler.doSomething().then(response => response.success);
  }
}
