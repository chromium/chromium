// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MostVisitedPageCallbackRouter, MostVisitedPageHandlerFactory, MostVisitedPageHandlerRemote} from './most_visited.mojom-webui.js';

export class MostVisitedBrowserProxy {
  handler: MostVisitedPageHandlerRemote;
  callbackRouter: MostVisitedPageCallbackRouter;

  constructor(
      handler: MostVisitedPageHandlerRemote,
      callbackRouter: MostVisitedPageCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): MostVisitedBrowserProxy {
    if (instance) {
      return instance;
    }
    const callbackRouter = new MostVisitedPageCallbackRouter();
    const handler = new MostVisitedPageHandlerRemote();
    const factory = MostVisitedPageHandlerFactory.getRemote();
    factory.createPageHandler(
        callbackRouter.$.bindNewPipeAndPassRemote(),
        handler.$.bindNewPipeAndPassReceiver());
    instance = new MostVisitedBrowserProxy(handler, callbackRouter);
    return instance;
  }

  static setInstance(obj: MostVisitedBrowserProxy) {
    instance = obj;
  }
}

let instance: MostVisitedBrowserProxy|null = null;
