// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the customize-themes component to
 * interact with the browser.
 */

import {CustomizeThemesClientCallbackRouter, CustomizeThemesHandlerFactory, CustomizeThemesHandlerInterface, CustomizeThemesHandlerRemote} from './customize_themes.mojom-webui.js';

export interface CustomizeThemesBrowserProxy {
  handler(): CustomizeThemesHandlerInterface;
  callbackRouter(): CustomizeThemesClientCallbackRouter;
  open(url: string): void;
}

export class CustomizeThemesBrowserProxyImpl implements
    CustomizeThemesBrowserProxy {
  private handler_: CustomizeThemesHandlerRemote;
  private callbackRouter_: CustomizeThemesClientCallbackRouter;

  constructor() {
    this.handler_ = new CustomizeThemesHandlerRemote();

    this.callbackRouter_ = new CustomizeThemesClientCallbackRouter();

    const factory = CustomizeThemesHandlerFactory.getRemote();
    factory.createCustomizeThemesHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());
  }

  handler() {
    return this.handler_;
  }

  callbackRouter() {
    return this.callbackRouter_;
  }

  open(url: string) {
    window.open(url, '_blank');
  }

  static getInstance(): CustomizeThemesBrowserProxy {
    return instance || (instance = new CustomizeThemesBrowserProxyImpl());
  }

  static setInstance(obj: CustomizeThemesBrowserProxy) {
    instance = obj;
  }
}

let instance: CustomizeThemesBrowserProxy|null = null;
