// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './searchbox.mojom-webui.js';
import {PageCallbackRouter, PageHandler} from './searchbox.mojom-webui.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the
 * <cr-searchbox> or the <cr-searchbox-dropdown> and the browser.
 */

let instance: SearchboxBrowserProxy|null = null;

export class SearchboxBrowserProxy {
  static getInstance(): SearchboxBrowserProxy {
    return instance || (instance = new SearchboxBrowserProxy());
  }

  static setInstance(newInstance: SearchboxBrowserProxy) {
    instance = newInstance;
  }

  handler: PageHandlerInterface;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.handler = PageHandler.getRemote();
    this.callbackRouter = new PageCallbackRouter();

    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }
}
