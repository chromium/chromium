// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the customize-color-scheme-mode
 * component to interact with the browser.
 */

import {CustomizeColorSchemeModeClientCallbackRouter, CustomizeColorSchemeModeHandlerFactory, CustomizeColorSchemeModeHandlerRemote} from './customize_color_scheme_mode.mojom-webui.js';

let instance: CustomizeColorSchemeModeBrowserProxy|null = null;

export class CustomizeColorSchemeModeBrowserProxy {
  handler: CustomizeColorSchemeModeHandlerRemote;
  callbackRouter: CustomizeColorSchemeModeClientCallbackRouter;

  private constructor(
      handler: CustomizeColorSchemeModeHandlerRemote,
      callbackRouter: CustomizeColorSchemeModeClientCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): CustomizeColorSchemeModeBrowserProxy {
    if (!instance) {
      const handler = new CustomizeColorSchemeModeHandlerRemote();
      const callbackRouter = new CustomizeColorSchemeModeClientCallbackRouter();
      CustomizeColorSchemeModeHandlerFactory.getRemote()
          .createCustomizeColorSchemeModeHandler(
              callbackRouter.$.bindNewPipeAndPassRemote(),
              handler.$.bindNewPipeAndPassReceiver());
      instance =
          new CustomizeColorSchemeModeBrowserProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: CustomizeColorSchemeModeHandlerRemote,
      callbackRouter: CustomizeColorSchemeModeClientCallbackRouter) {
    instance =
        new CustomizeColorSchemeModeBrowserProxy(handler, callbackRouter);
  }
}
