// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerRemote} from './history_clusters.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from './history_clusters.mojom-webui.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the page and
 * the browser.
 */

export interface BrowserProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;
}

export class BrowserProxyImpl implements BrowserProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  constructor(handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): BrowserProxy {
    if (instance) {
      return instance;
    }

    const handler = PageHandler.getRemote();
    const callbackRouter = new PageCallbackRouter();
    handler.setPage(callbackRouter.$.bindNewPipeAndPassRemote());
    return instance = new BrowserProxyImpl(handler, callbackRouter);
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
