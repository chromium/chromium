// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The browser proxy used to access `PageImageService` from WebUI.
 */

import {PageImageServiceHandler, PageImageServiceHandlerRemote} from './page_image_service.mojom-webui.js';

export class PageImageServiceBrowserProxy {
  handler: PageImageServiceHandlerRemote;

  constructor(handler: PageImageServiceHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): PageImageServiceBrowserProxy {
    return instance ||
        (instance = new PageImageServiceBrowserProxy(
             PageImageServiceHandler.getRemote()));
  }

  static setInstance(obj: PageImageServiceBrowserProxy) {
    instance = obj;
  }
}

let instance: PageImageServiceBrowserProxy|null = null;
