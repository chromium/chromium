// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the customize-themes component to
 * interact with the browser.
 */

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

import {CustomizeThemesClientCallbackRouter, CustomizeThemesHandlerFactory, CustomizeThemesHandlerFactoryRemote, CustomizeThemesHandlerInterface, CustomizeThemesHandlerRemote} from './customize_themes.mojom-webui.js';

/** @interface */
export class CustomizeThemesBrowserProxy {
  /** @return {CustomizeThemesHandlerInterface} */
  handler() {}

  /** @return {CustomizeThemesClientCallbackRouter} */
  callbackRouter() {}

  /** @param {string} url */
  open(url) {}
}

/** @implements {CustomizeThemesBrowserProxy} */
export class CustomizeThemesBrowserProxyImpl {
  constructor() {
    /** @private {!CustomizeThemesHandlerRemote} */
    this.handler_ = new CustomizeThemesHandlerRemote();

    /** @private {!CustomizeThemesClientCallbackRouter} */
    this.callbackRouter_ = new CustomizeThemesClientCallbackRouter();

    /** @type {!CustomizeThemesHandlerFactoryRemote} */
    const factory = CustomizeThemesHandlerFactory.getRemote();
    factory.createCustomizeThemesHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());
  }

  /** @override */
  handler() {
    return this.handler_;
  }

  /** @override */
  callbackRouter() {
    return this.callbackRouter_;
  }

  /** @override */
  open(url) {
    window.open(url, '_blank');
  }
}

addSingletonGetter(CustomizeThemesBrowserProxyImpl);
