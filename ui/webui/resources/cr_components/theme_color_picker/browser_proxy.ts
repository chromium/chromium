// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the cr-theme-color-picker
 * component to interact with the browser.
 */

import {ThemeColorPickerClientCallbackRouter, ThemeColorPickerHandlerFactory, ThemeColorPickerHandlerRemote} from './theme_color_picker.mojom-webui.js';

let instance: ThemeColorPickerBrowserProxy|null = null;

export class ThemeColorPickerBrowserProxy {
  handler: ThemeColorPickerHandlerRemote;
  callbackRouter: ThemeColorPickerClientCallbackRouter;

  private constructor(
      handler: ThemeColorPickerHandlerRemote,
      callbackRouter: ThemeColorPickerClientCallbackRouter) {
    this.handler = handler;
    this.callbackRouter = callbackRouter;
  }

  static getInstance(): ThemeColorPickerBrowserProxy {
    if (!instance) {
      const handler = new ThemeColorPickerHandlerRemote();
      const callbackRouter = new ThemeColorPickerClientCallbackRouter();
      ThemeColorPickerHandlerFactory.getRemote().createThemeColorPickerHandler(
          handler.$.bindNewPipeAndPassReceiver(),
          callbackRouter.$.bindNewPipeAndPassRemote());
      instance = new ThemeColorPickerBrowserProxy(handler, callbackRouter);
    }
    return instance;
  }

  static setInstance(
      handler: ThemeColorPickerHandlerRemote,
      callbackRouter: ThemeColorPickerClientCallbackRouter) {
    instance = new ThemeColorPickerBrowserProxy(handler, callbackRouter);
  }
}
