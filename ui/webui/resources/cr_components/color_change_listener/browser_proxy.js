// Copyright 2021 The Chromium Authors. All rights reserved.
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

/** @type {?BrowserProxy} */
let instance = null;

export class BrowserProxy {
  constructor() {
    /** @type {!PageCallbackRouter} */
    this.callbackRouter = new PageCallbackRouter();

    const pageHandlerRemote = PageHandler.getRemote();
    pageHandlerRemote.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  /** @return {!BrowserProxy} */
  static getInstance() {
    return instance || (instance = new BrowserProxy());
  }

  /**
   * @param {!BrowserProxy} newInstance
   */
  static setInstance(newInstance) {
    instance = newInstance;
  }
}
