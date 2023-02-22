// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the salient-image component to
 * interact with the browser.
 */

import {ImageServiceHandler, ImageServiceHandlerRemote} from './image_service.mojom-webui.js';

export class ImageServiceBrowserProxy {
  handler: ImageServiceHandlerRemote;

  constructor(handler: ImageServiceHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): ImageServiceBrowserProxy {
    return instance ||
        (instance =
             new ImageServiceBrowserProxy(ImageServiceHandler.getRemote()));
  }

  static setInstance(obj: ImageServiceBrowserProxy) {
    instance = obj;
  }
}

let instance: ImageServiceBrowserProxy|null = null;
