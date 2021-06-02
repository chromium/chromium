// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MostVisitedPageCallbackRouter, MostVisitedPageHandlerFactory, MostVisitedPageHandlerRemote} from './most_visited.mojom-webui.js';

export class MostVisitedBrowserProxy {
  /**
   * @param {!MostVisitedPageHandlerRemote} handler
   * @param {!MostVisitedPageCallbackRouter} callbackRouter
   */
  constructor(handler, callbackRouter) {
    /** @type {!MostVisitedPageHandlerRemote} */
    this.handler = handler;

    /** @type {!MostVisitedPageCallbackRouter} */
    this.callbackRouter = callbackRouter;
  }

  /** @return {!MostVisitedBrowserProxy} */
  static getInstance() {
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

  /**
   * @param {!MostVisitedBrowserProxy} obj
   */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?MostVisitedBrowserProxy} */
let instance = null;
