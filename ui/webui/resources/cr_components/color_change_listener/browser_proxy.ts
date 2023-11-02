// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for one way communication between the JS and the
 * browser.
 * TODO(tluk): Convert this into typescript once all dependencies have been
 * fully migrated.
 */

import {PageCallbackRouter, PageHandler} from './color_change_listener.mojom-webui.js';

let instance: BrowserProxy|null = null;

export class BrowserProxy {
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    const pageHandlerRemote = PageHandler.getRemote();
    pageHandlerRemote.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(newInstance: BrowserProxy) {
    instance = newInstance;
  }
}
