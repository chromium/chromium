// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the customize-themes component to
 * interact with the browser.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-lite.js';
import './customize_themes.mojom-lite.js';

import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

/** @interface */
export class CustomizeThemesBrowserProxy {
  /** @return {customizeThemes.mojom.CustomizeThemesHandlerInterface} */
  handler() {}

  /** @return {customizeThemes.mojom.CustomizeThemesClientCallbackRouter} */
  callbackRouter() {}

  /** @param {string} url */
  open(url) {}
}

/** @implements {CustomizeThemesBrowserProxy} */
export class CustomizeThemesBrowserProxyImpl {
  constructor() {
    /** @private {customizeThemes.mojom.CustomizeThemesHandlerRemote} */
    this.handler_ = new customizeThemes.mojom.CustomizeThemesHandlerRemote();

    /** @private {customizeThemes.mojom.CustomizeThemesClientCallbackRouter} */
    this.callbackRouter_ =
        new customizeThemes.mojom.CustomizeThemesClientCallbackRouter();

    /** @type {customizeThemes.mojom.CustomizeThemesHandlerFactoryRemote} */
    const factory =
        customizeThemes.mojom.CustomizeThemesHandlerFactory.getRemote();
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
