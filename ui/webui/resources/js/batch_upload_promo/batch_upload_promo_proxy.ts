// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from '../batch_upload_promo.mojom-webui.js';

let instance: BatchUploadPromoProxy|null = null;

/** Holds Mojo interfaces for communication with the browser process. */
export class BatchUploadPromoProxy {
  handler: PageHandlerRemote;
  callbackRouter: PageCallbackRouter;

  private constructor(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): BatchUploadPromoProxy {
    if (!instance) {
      const handler = new PageHandlerRemote();
      const callbackRouter = new PageCallbackRouter();
      PageHandlerFactory.getRemote().createBatchUploadPromoHandler(
          callbackRouter.$.bindNewPipeAndPassRemote(),
          handler.$.bindNewPipeAndPassReceiver());
      instance = new BatchUploadPromoProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: PageHandlerRemote, callbackRouter: PageCallbackRouter) {
    instance = new BatchUploadPromoProxy(handler, callbackRouter);
  }
}
