// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/skia/public/mojom/image_info.mojom-lite.js';
import 'chrome://resources/mojo/skia/public/mojom/bitmap.mojom-lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import './file_path.mojom-lite.js';
import './image.mojom-lite.js';
import './types.mojom-lite.js';
import './app_management.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

export class BrowserProxy {
  constructor() {
    /** @type {appManagement.mojom.PageCallbackRouter} */
    this.callbackRouter = new appManagement.mojom.PageCallbackRouter();

    /** @type {appManagement.mojom.PageHandlerRemote} */
    this.handler = new appManagement.mojom.PageHandlerRemote();
    const factory = appManagement.mojom.PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}

addSingletonGetter(BrowserProxy);
