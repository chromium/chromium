// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HelpBubbleHandlerInterface} from './help_bubble.mojom-webui.js';
import {HelpBubbleClientCallbackRouter, HelpBubbleHandlerFactory, HelpBubbleHandlerRemote} from './help_bubble.mojom-webui.js';

export interface HelpBubbleProxy {
  getHandler(): HelpBubbleHandlerInterface;
  getCallbackRouter(): HelpBubbleClientCallbackRouter;
}

export class HelpBubbleProxyImpl implements HelpBubbleProxy {
  private callbackRouter_ = new HelpBubbleClientCallbackRouter();
  private handler_ = new HelpBubbleHandlerRemote();

  constructor() {
    const factory = HelpBubbleHandlerFactory.getRemote();
    factory.createHelpBubbleHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.handler_.$.bindNewPipeAndPassReceiver());
  }

  static getInstance(): HelpBubbleProxy {
    return instance || (instance = new HelpBubbleProxyImpl());
  }

  static setInstance(obj: HelpBubbleProxy) {
    instance = obj;
  }

  getHandler(): HelpBubbleHandlerRemote {
    return this.handler_;
  }

  getCallbackRouter(): HelpBubbleClientCallbackRouter {
    return this.callbackRouter_;
  }
}

let instance: HelpBubbleProxy|null = null;